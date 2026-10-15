// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/quick_layout.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <limits>

namespace cutefish::ui {

namespace qml {

// qRound: halves away from zero.
double qRound(double value)
{
    return value >= 0 ? std::floor(value + 0.5) : std::ceil(value - 0.5);
}

double anchorsCenter(double parentExtent, double itemExtent)
{
    return qRound(parentExtent / 2) - qRound(itemExtent / 2);
}

} // namespace qml

// QGridLayoutBox: what a row or column can take.
namespace {

struct Box
{
    double minimum = 0;
    double preferred = 0;
    double maximum = FLT_MAX;

    // QGridLayoutBox::combine, for items sharing a row or column.
    void combine(const Box& other)
    {
        minimum = std::max(minimum, other.minimum);
        double maxMax;
        if (maximum == FLT_MAX && other.maximum != FLT_MAX)
            maxMax = other.maximum;
        else if (other.maximum == FLT_MAX && maximum != FLT_MAX)
            maxMax = maximum;
        else
            maxMax = std::max(maximum, other.maximum);
        maximum = std::max(minimum, maxMax);
        preferred = std::clamp(std::max(preferred, other.preferred), minimum, maximum);
    }
};

} // namespace

static constexpr double kItemSizeMax = 1 << 24;

static double qround(double value)
{
    return qml::qRound(value);
}

// QGridLayoutRowData::totalBox; unset stretch adds maximum sizes.
static Box totalBox(const std::vector<Box>& boxes, double spacing)
{
    Box total{0, 0, 0};
    for (size_t i = 0; i < boxes.size(); ++i) {
        const double gap = i == 0 ? 0 : spacing;
        total.minimum += boxes[i].minimum + gap;
        total.preferred += boxes[i].preferred + gap;
        total.maximum += boxes[i].maximum + gap;
    }
    return total;
}

static double growthFactorBelowPreferredSize(double desired, double sumAvailable, double sumDesired)
{
    return desired * std::pow(sumAvailable / sumDesired, desired / sumDesired);
}

// QGridLayoutRowData::calculateGeometries for one cell per item, without
// spans, descents or ignored rows, and stretch factors left unset.
static void calculateGeometries(const std::vector<Box>& boxes, double spacing, double targetSize,
                                std::vector<double>& positions, std::vector<double>& sizes)
{
    const size_t n = boxes.size();
    positions.assign(n, 0);
    sizes.assign(n, 0);
    if (n == 0)
        return;

    const Box total = totalBox(boxes, spacing);
    targetSize = std::max(total.minimum, targetSize);
    const auto stealBox = [&](int which) {
        double offset = 0;
        for (size_t i = 0; i < n; ++i) {
            if (i > 0)
                offset += spacing;
            positions[i] = offset;
            sizes[i] = which == 0 ? boxes[i].minimum : which == 1 ? boxes[i].preferred : boxes[i].maximum;
            offset += sizes[i];
        }
    };

    std::vector<double> newSizes(n, 0);
    std::vector<double> factors(n, 0);
    double sumFactors = 0;
    double sumAvailable = 0;

    if (targetSize < total.preferred) {
        stealBox(0);
        sumAvailable = targetSize - total.minimum;
        if (sumAvailable > 0) {
            const double sumDesired = std::min(total.preferred, kItemSizeMax) - total.minimum;
            for (size_t i = 0; i < n; ++i) {
                const double desired = std::min(boxes[i].preferred, kItemSizeMax) - boxes[i].minimum;
                factors[i] = growthFactorBelowPreferredSize(desired, sumAvailable, sumDesired);
                sumFactors += factors[i];
            }
            for (size_t i = 0; i < n; ++i)
                newSizes[i] = sizes[i] + (sumFactors > 0 ? sumAvailable * factors[i] / sumFactors : 0);
        }
    } else {
        const bool largerThanMaximum = targetSize > total.maximum;
        if (largerThanMaximum) {
            stealBox(2);
            sumAvailable = targetSize - total.maximum;
        } else {
            stealBox(1);
            sumAvailable = targetSize - total.preferred;
        }

        if (sumAvailable > 0) {
            double sumCurrentAvailable = sumAvailable;
            bool somethingHasAMaximumSize = false;
            for (size_t i = 0; i < n; ++i) {
                // Past every maximum the cells still grow, up to the rows'
                // own maximum, which nothing sets; their items stay put.
                const double boxSize = largerThanMaximum ? boxes[i].maximum : boxes[i].preferred;
                const double desired = (largerThanMaximum ? FLT_MAX : boxes[i].maximum) - boxSize;
                if (desired == 0) {
                    newSizes[i] = sizes[i];
                    factors[i] = 0;
                } else {
                    // Without stretch factors, growth follows the current size.
                    factors[i] = sizes[i] == 0 ? 1.0 : sizes[i];
                    sumFactors += factors[i];
                    if (desired < sumCurrentAvailable)
                        somethingHasAMaximumSize = true;
                    newSizes[i] = -1;
                }
            }

            bool keepGoing = somethingHasAMaximumSize;
            while (keepGoing) {
                keepGoing = false;
                for (size_t i = 0; i < n; ++i) {
                    if (newSizes[i] >= 0)
                        continue;
                    const double maxBoxSize =
                        std::max(boxes[i].minimum, std::floor(largerThanMaximum ? FLT_MAX : boxes[i].maximum));
                    const double avail = sumCurrentAvailable * factors[i] / sumFactors;
                    if (sizes[i] + avail >= maxBoxSize) {
                        newSizes[i] = maxBoxSize;
                        sumCurrentAvailable -= maxBoxSize - sizes[i];
                        sumFactors -= factors[i];
                        keepGoing = sumCurrentAvailable > 0;
                        if (!keepGoing)
                            break;
                    }
                }
            }
            for (size_t i = 0; i < n; ++i) {
                if (newSizes[i] < 0)
                    newSizes[i] = sizes[i] + (sumFactors == 0 ? 0 : sumCurrentAvailable * factors[i] / sumFactors);
            }
        }
    }

    if (sumAvailable > 0) {
        double offset = 0;
        for (size_t i = 0; i < n; ++i) {
            const double delta = newSizes[i] - sizes[i];
            positions[i] += offset;
            sizes[i] += delta;
            offset += delta;
        }
    }

    // snapToPixelGrid.
    for (size_t i = 0; i < n; ++i) {
        const double old = positions[i];
        positions[i] = qround(old);
        const double delta = positions[i] - old;
        sizes[i] -= delta;
        if (i > 0)
            sizes[i - 1] += delta;
    }
    sizes[n - 1] = targetSize - positions[n - 1];
    for (size_t i = 0; i < n; ++i)
        sizes[i] = std::max(boxes[i].minimum, qround(sizes[i]));
}

// A padded layout stands for the item it is anchored in, not a layout.
static const QuickLayout* nestedLayout(const View* view)
{
    const auto* layout = dynamic_cast<const QuickLayout*>(view);
    return layout && layout->padding() == Margins{} ? layout : nullptr;
}

// A nested Qt Quick layout fills unless told otherwise.
static bool fills(const View* view, bool horizontal)
{
    const LayoutHints& hints = view->layoutHints();
    return (horizontal ? hints.fillWidth : hints.fillHeight) || nestedLayout(view);
}

// QQuickLayout::effectiveSizeHints_helper and QGridLayoutItem::box: the
// item's box on one axis, its margins included; `constraint` is the width
// a height is measured for.
static Box itemBox(const View* view, bool horizontal, double constraint)
{
    const LayoutHints& hints = view->layoutHints();
    double minimum = horizontal ? hints.minimumWidth : hints.minimumHeight;
    double preferred = horizontal ? hints.preferredWidth : hints.preferredHeight;
    double maximum = horizontal ? hints.maximumWidth : hints.maximumHeight;
    const double margins = horizontal ? hints.margins.horizontal() : hints.margins.vertical();
    const double marginsAcross = horizontal ? 0 : hints.margins.horizontal();

    // normalizeHints on what was set.
    if (minimum >= 0 && maximum >= 0 && minimum > maximum)
        minimum = maximum;
    if (preferred >= 0) {
        if (minimum >= 0 && preferred < minimum)
            preferred = minimum;
        if (maximum >= 0 && preferred > maximum)
            preferred = maximum;
    }

    // A nested layout's implicit minimum and maximum are what its children
    // take, as QQuickLayoutPrivate::applySizeHints sets them.
    const QuickLayout* layout = nestedLayout(view);
    QuickLayout::Extent extent;
    if (layout) {
        extent = horizontal ? layout->horizontalExtent()
                            : layout->verticalExtent(constraint >= 0 ? std::max(0.0, constraint - marginsAcross) : -1);
    }
    if (maximum < 0)
        maximum = layout ? extent.maximum : std::numeric_limits<double>::infinity();
    maximum = std::max({maximum, preferred, minimum});
    if (minimum < 0 && layout)
        minimum = extent.minimum;
    minimum = std::max(minimum, 0.0);
    if (preferred >= 0)
        minimum = std::min(minimum, preferred);
    minimum = std::min(minimum, maximum);
    if (preferred < 0) {
        double implicit = extent.preferred;
        if (!layout) {
            implicit = horizontal ? view->sizeHint().width
                                  : view->heightForWidth(constraint >= 0 ? std::max(0.0, constraint - marginsAcross)
                                                                         : view->sizeHint().width);
        }
        const double fallback = horizontal ? hints.fallbackWidth : hints.fallbackHeight;
        preferred = implicit > 0 ? std::ceil(implicit) : std::max(fallback, 0.0);
    }
    preferred = std::clamp(preferred, minimum, maximum);

    Box box{minimum + margins, preferred + margins, maximum + margins};
    if (!fills(view, horizontal)) {
        box.minimum = box.preferred;
        box.maximum = box.preferred;
    }
    // An unset maximum stays infinite, which QGridLayoutBox::combine keeps
    // apart from the FLT_MAX of an empty row.
    box.minimum = std::ceil(box.minimum);
    return box;
}

// The width QGridLayoutItem::geometryWithin gives an item in its cell, its
// margins included: a filling item takes the cell up to its maximum.
static double itemWidth(const View* view, double cellWidth)
{
    const Box box = itemBox(view, true, -1);
    return std::min(fills(view, true) ? box.maximum : box.preferred, cellWidth);
}

QuickLayout::QuickLayout(Flow flow, int columns)
    : m_flow(flow)
    , m_columns(columns)
{
}

void QuickLayout::setSpacing(double spacing)
{
    m_rowSpacing = spacing;
    m_columnSpacing = spacing;
    requestLayout();
}

void QuickLayout::setRowSpacing(double spacing)
{
    m_rowSpacing = spacing;
    requestLayout();
}

void QuickLayout::setColumnSpacing(double spacing)
{
    m_columnSpacing = spacing;
    requestLayout();
}

void QuickLayout::setPadding(const Margins& padding)
{
    m_padding = padding;
    requestLayout();
}

void QuickLayout::setColumnCount(int columns)
{
    m_columns = columns;
    requestLayout();
}

std::vector<QuickLayout::Cell> QuickLayout::cells() const
{
    std::vector<Cell> cells;
    int index = 0;
    for (const auto& child : children()) {
        if (!child->isVisible())
            continue;
        switch (m_flow) {
        case Flow::Row:
            cells.push_back({child.get(), 0, index});
            break;
        case Flow::Column:
            cells.push_back({child.get(), index, 0});
            break;
        case Flow::Grid:
            cells.push_back({child.get(), m_columns > 0 ? index / m_columns : 0,
                             m_columns > 0 ? index % m_columns : index});
            break;
        }
        ++index;
    }
    return cells;
}

QuickLayout::Geometry QuickLayout::solve(const std::vector<Cell>& cells, double width, double height) const
{
    Geometry geometry;
    int rows = 0;
    int columns = 0;
    for (const Cell& cell : cells) {
        rows = std::max(rows, cell.row + 1);
        columns = std::max(columns, cell.column + 1);
    }

    // Columns first, as QGridLayoutEngine::ensureGeometries; heights are
    // measured at the column widths.
    std::vector<Box> columnBoxes(static_cast<size_t>(columns));
    for (const Cell& cell : cells)
        columnBoxes[size_t(cell.column)].combine(itemBox(cell.view, true, -1));
    const Box horizontal = totalBox(columnBoxes, m_columnSpacing);
    geometry.horizontal = {horizontal.minimum + m_padding.horizontal(), horizontal.preferred + m_padding.horizontal(),
                           horizontal.maximum + m_padding.horizontal()};
    calculateGeometries(columnBoxes, m_columnSpacing, width < 0 ? horizontal.preferred : width, geometry.x,
                        geometry.widths);

    std::vector<Box> rowBoxes(static_cast<size_t>(rows));
    for (const Cell& cell : cells)
        rowBoxes[size_t(cell.row)].combine(itemBox(cell.view, false, itemWidth(cell.view, geometry.widths[size_t(cell.column)])));
    const Box vertical = totalBox(rowBoxes, m_rowSpacing);
    geometry.vertical = {vertical.minimum + m_padding.vertical(), vertical.preferred + m_padding.vertical(),
                         vertical.maximum + m_padding.vertical()};
    calculateGeometries(rowBoxes, m_rowSpacing, height < 0 ? vertical.preferred : height, geometry.y, geometry.heights);
    return geometry;
}

QuickLayout::Extent QuickLayout::horizontalExtent() const
{
    const std::vector<Cell> items = cells();
    if (items.empty())
        return {m_padding.horizontal(), m_padding.horizontal(), m_padding.horizontal()};
    return solve(items, -1, -1).horizontal;
}

QuickLayout::Extent QuickLayout::verticalExtent(double width) const
{
    const std::vector<Cell> items = cells();
    if (items.empty())
        return {m_padding.vertical(), m_padding.vertical(), m_padding.vertical()};
    return solve(items, width < 0 ? -1 : std::max(0.0, width - m_padding.horizontal()), -1).vertical;
}

// The implicit size, as a QML layout's: its children's preferred total.
SizeF QuickLayout::sizeHint() const
{
    const std::vector<Cell> items = cells();
    if (items.empty())
        return {m_padding.horizontal(), m_padding.vertical()};
    const Geometry geometry = solve(items, -1, -1);
    return {geometry.horizontal.preferred, geometry.vertical.preferred};
}

double QuickLayout::heightForWidth(double width) const
{
    return verticalExtent(width).preferred;
}

void QuickLayout::layout()
{
    const std::vector<Cell> items = cells();
    if (items.empty())
        return;
    const RectF bounds = localRect();
    const Geometry geometry = solve(items, std::max(0.0, bounds.width - m_padding.horizontal()),
                                    std::max(0.0, bounds.height - m_padding.vertical()));

    for (const Cell& cell : items) {
        View* view = cell.view;
        const LayoutHints& hints = view->layoutHints();
        const double cellX = geometry.x[size_t(cell.column)];
        const double cellY = geometry.y[size_t(cell.row)];
        const double cellWidth = geometry.widths[size_t(cell.column)];
        const double cellHeight = geometry.heights[size_t(cell.row)];

        // QGridLayoutItem::geometryWithin: a filling item takes its cell up to
        // its maximum, another its preferred size.
        const double width = itemWidth(view, cellWidth);
        const Box vertical = itemBox(view, false, width);
        double height = fills(view, false) ? vertical.maximum : vertical.preferred;
        height = std::min(height, cellHeight);

        double x = cellX;
        double y = cellY;
        switch (hints.horizontalAlignment) {
        case Alignment::Center:
            x += (cellWidth - width) / 2;
            break;
        case Alignment::End:
            x += cellWidth - width;
            break;
        default:
            break;
        }
        switch (hints.verticalAlignment) {
        case Alignment::Start:
            break;
        case Alignment::End:
            y += cellHeight - height;
            break;
        default:
            y += (cellHeight - height) / 2;
            break;
        }
        // QGridLayoutEngine::setGeometries rounds, then the item's margins
        // come off; the padding is the anchors' margins, not rounded.
        x = qround(x) + m_padding.left;
        y = qround(y) + m_padding.top;
        view->setGeometry({x + hints.margins.left, y + hints.margins.top,
                           std::max(0.0, width - hints.margins.horizontal()),
                           std::max(0.0, height - hints.margins.vertical())});
    }
}

} // namespace cutefish::ui
