// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/segmented_control.h"

#include <algorithm>
#include <cmath>

#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"

namespace cutefish::ui {

// FishUI's TabButton: a medium icon and a small spacing tall.
static constexpr double kHeight = 38;
static constexpr double kPadding = 2;

SegmentedControl::SegmentedControl(std::vector<std::string> items, Style style)
    : m_style(style)
{
    setItems(std::move(items));
}

void SegmentedControl::setItems(std::vector<std::string> items)
{
    for (Label* label : m_labels)
        takeChild(label);
    m_labels.clear();
    for (std::string& item : items) {
        Label* label = addChild<Label>(std::move(item));
        label->setTextAlignment(Label::TextAlignment::Center);
        label->setVerticallyCentered(true);
        m_labels.push_back(label);
    }
    m_current = std::clamp(m_current, 0, std::max(0, int(m_labels.size()) - 1));
    updateColors();
    requestLayout();
}

void SegmentedControl::setCurrentIndex(int index)
{
    if (index < 0 || index >= int(m_labels.size()) || index == m_current)
        return;
    m_current = index;
    requestPaint();
}

SizeF SegmentedControl::sizeHint() const
{
    double widest = 0;
    for (const Label* label : m_labels)
        widest = std::max(widest, label->sizeHint().width);
    const double item = widest + 2 * Theme::current().largeSpacing;
    return {item * double(m_labels.size()) + 2 * kPadding, kHeight};
}

RectF SegmentedControl::itemRect(size_t index) const
{
    const RectF inner = localRect().adjusted(kPadding, kPadding, -kPadding, -kPadding);
    const double width = m_labels.empty() ? 0 : inner.width / double(m_labels.size());
    return {inner.x + width * double(index), inner.y, width, inner.height};
}

int SegmentedControl::itemAt(PointF point) const
{
    for (size_t i = 0; i < m_labels.size(); ++i) {
        if (itemRect(i).contains(point))
            return int(i);
    }
    return -1;
}

void SegmentedControl::layout()
{
    for (size_t i = 0; i < m_labels.size(); ++i)
        m_labels[i]->setGeometry(itemRect(i));
}

void SegmentedControl::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const RectF bounds = localRect();
    const Color plate = theme.darkMode ? theme.secondBackground.lighter(2) : theme.secondBackground;
    if (m_style == Style::Segmented) {
        painter.fillRoundedRect(bounds, bounds.height / 2,
                                theme.darkMode ? Color::rgb(0xFFFFFF).withAlpha(0.06)
                                               : Color::rgb(0x000000).withAlpha(0.04));
    } else {
        painter.fillRoundedRect(bounds, theme.smallRadius + kPadding, theme.alternateBackground);
    }
    if (m_current < 0 || m_current >= int(m_labels.size()))
        return;

    const RectF highlight = itemRect(size_t(m_current));
    if (m_style == Style::Segmented) {
        painter.drawShadow(highlight, highlight.height / 2, 1.5,
                           Color::rgb(0).withAlpha(theme.darkMode ? 0.18 : 0.12));
        painter.fillRoundedRect(highlight, highlight.height / 2, plate);
    } else {
        // Concentric with the bar: its radius less the inset.
        if (theme.darkMode)
            painter.drawShadow(highlight, theme.smallRadius, 1, Color::rgb(0).withAlpha(0.11));
        painter.fillRoundedRect(highlight, theme.smallRadius, plate);
    }
}

void SegmentedControl::updateColors()
{
    const Theme& theme = Theme::current();
    for (size_t i = 0; i < m_labels.size(); ++i) {
        if (int(i) == m_pressed)
            m_labels[i]->setColor(theme.text.withAlpha(0.5));
        else
            m_labels[i]->setColor(std::nullopt);
    }
}

void SegmentedControl::pick(int index)
{
    if (index < 0 || index >= int(m_labels.size()) || index == m_current)
        return;
    m_current = index;
    requestPaint();
    if (onCurrentChanged)
        onCurrentChanged(index);
}

void SegmentedControl::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    m_pressed = itemAt(event.position);
    updateColors();
}

void SegmentedControl::pointerRelease(const PointerEvent& event)
{
    const int pressed = m_pressed;
    m_pressed = -1;
    updateColors();
    if (pressed >= 0 && itemAt(event.position) == pressed)
        pick(pressed);
}

bool SegmentedControl::keyPress(const KeyEvent& event)
{
    if (event.modifiers != NoModifier)
        return false;
    if (event.key == key::Left) {
        pick(m_current - 1);
        return true;
    }
    if (event.key == key::Right) {
        pick(m_current + 1);
        return true;
    }
    return false;
}

} // namespace cutefish::ui
