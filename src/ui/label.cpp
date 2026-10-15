// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/label.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "include/core/SkCanvas.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkImage.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTypeface.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"

#include "ui/application.h"
#include "ui/painter.h"
#include "ui/text_context.h"
#include "ui/text_lines.h"
#include "ui/theme.h"
#include "ui/window.h"

namespace textlayout = skia::textlayout;

namespace cutefish::ui {

// Wide enough that nothing wraps while measuring.
static constexpr double kUnboundedWidth = 1e6;
static constexpr const char* kEllipsis = "…";
// Ganesh draws text from 162 device pixels as distance fields; a pixel of margin.
static constexpr double kDistanceFieldTextSize = 161;

Label::Label(std::string text)
    : m_text(std::move(text))
{
}

Label::~Label() = default;

void Label::setText(std::string text)
{
    if (text == m_text && m_spans.empty())
        return;
    m_text = std::move(text);
    m_spans.clear();
    invalidate();
}

void Label::setSpans(std::vector<Span> spans)
{
    m_text.clear();
    for (const Span& span : spans)
        m_text += span.text;
    m_spans = std::move(spans);
    invalidate();
}

void Label::setFontSize(double size)
{
    if (size == m_fontSize)
        return;
    m_fontSize = size;
    invalidate();
}

void Label::setFontFeatures(std::vector<std::pair<std::string, int>> features)
{
    m_features = std::move(features);
    invalidate();
}

void Label::setFontFamily(std::string family)
{
    if (family == m_family)
        return;
    m_family = std::move(family);
    invalidate();
}

void Label::setFadesWhenDisabled(bool fades)
{
    m_fadesWhenDisabled = fades;
    requestPaint();
}

void Label::setWeight(int weight)
{
    if (weight == m_weight)
        return;
    m_weight = weight;
    invalidate();
}

void Label::setColor(std::optional<Color> color)
{
    if (color == m_color)
        return;
    m_color = color;
    requestPaint();
}

void Label::setWrap(bool wrap)
{
    if (wrap == m_wrap)
        return;
    m_wrap = wrap;
    invalidate();
}

void Label::setElide(Elide elide)
{
    if (elide == m_elide)
        return;
    m_elide = elide;
    invalidate();
}

void Label::setMaximumLineCount(int count)
{
    if (count == m_maximumLineCount)
        return;
    m_maximumLineCount = count;
    invalidate();
}

void Label::setTextAlignment(TextAlignment alignment)
{
    if (alignment == m_alignment)
        return;
    m_alignment = alignment;
    requestPaint();
}

void Label::setVerticallyCentered(bool centered)
{
    m_verticallyCentered = centered;
    requestPaint();
}

void Label::setBaseline(double y)
{
    if (y == m_baseline)
        return;
    m_baseline = y;
    requestLayout();
}

double Label::currentScale() const
{
    const Window* window = this->window();
    return window ? window->scale() : 1.0;
}

double Label::pixelSize() const
{
    return TextContext::pixelSize(m_fontSize > 0 ? m_fontSize : Theme::current().fontSize());
}

// FishUI.Dpi's snap(): onto the device grid.
double Label::snapped(double value) const
{
    const double scale = currentScale();
    return std::round(value * scale) / scale;
}

double Label::ascent() const
{
    const TextContext& context = Application::instance()->text();
    return context.metrics(context.typeface(m_weight, m_family).get(), pixelSize(), currentScale()).ascent;
}

double Label::descent() const
{
    const TextContext& context = Application::instance()->text();
    return context.metrics(context.typeface(m_weight, m_family).get(), pixelSize(), currentScale()).descent;
}

double Label::xHeight() const
{
    const TextContext& context = Application::instance()->text();
    return context.metrics(context.typeface(m_weight, m_family).get(), pixelSize(), currentScale()).xHeight;
}

double Label::baselinePadding() const
{
    if (m_baseline < 0)
        return 0;
    // AlignedLabel's Layout.topMargin.
    return std::max(0.0, snapped(m_baseline - ascent()));
}

double Label::textOffset(double textHeight) const
{
    if (m_baseline >= 0)
        return baselinePadding();
    // Qt Text's AlignVCenter: half the space left, not snapped.
    if (m_verticallyCentered)
        return (geometry().height - textHeight) / 2;
    return 0;
}

bool Label::roundsAdvances() const
{
    return Application::instance()->text().roundsMetrics(currentScale());
}

double Label::slack() const
{
    textlayout::Paragraph* text = paragraph();
    const bool round = roundsAdvances();
    if (m_naturalWidth < 0 || m_naturalRounded != round) {
        if (m_layoutWidth != kUnboundedWidth) {
            text->layout(SkScalar(kUnboundedWidth));
            m_layoutWidth = kUnboundedWidth;
        }
        m_naturalWidth = naturalWidth(text, round);
        m_intrinsicWidth = text->getMaxIntrinsicWidth();
        m_naturalRounded = round;
    }
    return m_intrinsicWidth - m_naturalWidth;
}

SizeF Label::sizeHint() const
{
    textlayout::Paragraph* text = layoutParagraph(kUnboundedWidth);
    const TextContext& context = Application::instance()->text();
    const std::vector<TextLineBox> lines =
        textLineBoxes(text, context.typeface(m_weight, m_family).get(), pixelSize(), currentScale());
    slack();
    // Qt Text's implicit size: the natural width, unrounded.
    return {m_naturalWidth, textHeight(lines) + baselinePadding()};
}

double Label::heightForWidth(double width) const
{
    textlayout::Paragraph* text = layoutParagraph(m_wrap ? width : kUnboundedWidth);
    const TextContext& context = Application::instance()->text();
    const std::vector<TextLineBox> lines =
        textLineBoxes(text, context.typeface(m_weight, m_family).get(), pixelSize(), currentScale());
    return textHeight(lines) + baselinePadding();
}

double Label::baselineOffset() const
{
    textlayout::Paragraph* text = layoutParagraph(m_wrap ? geometry().width : kUnboundedWidth);
    const TextContext& context = Application::instance()->text();
    const std::vector<TextLineBox> lines =
        textLineBoxes(text, context.typeface(m_weight, m_family).get(), pixelSize(), currentScale());
    return textOffset(textHeight(lines)) + lines.front().ascent;
}

double Label::capHeight() const
{
    const TextContext& context = Application::instance()->text();
    const textlayout::TextStyle style = context.style(m_fontSize, m_weight, Theme::current().text, m_family);
    textlayout::ParagraphStyle paragraphStyle;
    paragraphStyle.setTextStyle(style);
    auto builder = textlayout::ParagraphBuilder::make(paragraphStyle, context.fontCollection(), context.unicode());
    builder->pushStyle(style);
    builder->addText("H", 1);
    builder->pop();
    std::unique_ptr<textlayout::Paragraph> capital = builder->Build();
    capital->layout(SkScalar(kUnboundedWidth));
    return ui::tightBoundingRect(capital.get(), pixelSize(), currentScale()).height;
}

double Label::capCenter() const
{
    const double baseline = m_baseline >= 0 ? m_baseline : baselineOffset();
    return snapped(snapped(baseline) - capHeight() / 2);
}

RectF Label::tightBoundingRect() const
{
    if (m_text.empty())
        return {};
    return ui::tightBoundingRect(layoutParagraph(kUnboundedWidth), pixelSize(), currentScale());
}

bool Label::isElided() const
{
    if (m_wrap)
        return m_wrappedWidth >= 0 && m_wrappedElided;
    if (!m_paragraph || m_layoutWidth < 0 || m_elide == Elide::None)
        return false;
    if (m_elided && m_elidedWidth == m_layoutWidth)
        return true;
    return m_paragraph->didExceedMaxLines();
}

void Label::paint(Painter& painter)
{
    paintText(painter, {}, std::nullopt);
}

void Label::paintText(Painter& painter, PointF shift, std::optional<Color> color)
{
    if (m_text.empty())
        return;
    const double scale = painter.scale();
    double width = geometry().width;
    if (!m_wrap && m_elide == Elide::None) {
        // Runs past the label; the intrinsic width needs a layout at any width.
        paragraph();
        if (m_layoutWidth < 0)
            layoutParagraph(width);
        width = std::max(width, naturalWidth(m_paragraph.get(), roundsAdvances()));
    }
    textlayout::Paragraph* text = layoutParagraph(width);
    const TextContext& context = Application::instance()->text();
    const std::vector<TextLineBox> lines = textLineBoxes(text, context.typeface(m_weight, m_family).get(), pixelSize(), scale);
    const double offset = textOffset(textHeight(lines));

    const Theme& theme = Theme::current();
    const Color textColor = color.value_or(m_color.value_or(theme.text));
    const bool wrapped = m_wrapped && text == m_wrapped.get() && !m_wrappedElided;
    const bool spans = !color && !m_spans.empty() && (text == m_paragraph.get() || wrapped);
    const auto colorAt = [&](size_t utf8Offset) -> SkColor {
        if (wrapped) {
            // Back from the text with the newlines put in.
            size_t inserted = 0;
            while (inserted < m_wrappedBreaks.size() && m_wrappedBreaks[inserted] + inserted < utf8Offset)
                ++inserted;
            utf8Offset -= inserted;
        }
        if (spans) {
            size_t start = 0;
            for (const Span& span : m_spans) {
                if (utf8Offset < start + span.text.size())
                    return span.color.value_or(textColor).argb();
                start += span.text.size();
            }
        }
        return textColor.argb();
    };

    SkCanvas* canvas = painter.canvas();
    // FishUI.Label keeps its colour and fades when disabled. A layer only
    // when faded: glyphs blend themselves otherwise.
    const double opacity = painter.opacity() * (isEnabled() || !m_fadesWhenDisabled ? 1 : 0.6);
    const bool faded = opacity < 1;
    if (faded) {
        // Bounded by the text, with room for overhangs: an unbounded layer
        // is the whole window for every faded label.
        const double margin = pixelSize();
        const double right = std::max(width, geometry().width);
        const double bottom = std::max(geometry().height, offset + textHeight(lines));
        const SkRect bounds = SkRect::MakeLTRB(SkScalar((painter.offset().x - margin) * scale),
                                               SkScalar((painter.offset().y - margin) * scale),
                                               SkScalar((painter.offset().x + right + margin) * scale),
                                               SkScalar((painter.offset().y + bottom + margin) * scale));
        canvas->saveLayerAlphaf(&bounds, float(opacity));
    }
    const double align = m_alignment == TextAlignment::Left ? 0 : m_alignment == TextAlignment::Center ? 0.5 : 1;
    // Glyphs go where Qt puts them, in logical pixels from the label's
    // unsnapped origin; their baselines land on device pixels.
    const auto paintText = [&](SkCanvas* target) {
        target->save();
        target->translate(SkScalar(painter.offset().x * scale), SkScalar(painter.offset().y * scale));
        target->scale(SkScalar(scale), SkScalar(scale));
        paintTextLines(target, text, lines, {shift.x, offset + shift.y}, geometry().width, align, roundsAdvances(),
                       colorAt);
        target->restore();
    };
    // Ganesh draws glyphs this large as unhinted distance fields; the raster
    // backend's hinted masks, drawn aside, keep GPU frames like CPU ones.
    if (canvas->recordingContext() && pixelSize() * scale >= kDistanceFieldTextSize) {
        const double margin = pixelSize();
        const double right = std::max(width, geometry().width);
        const double bottom = std::max(geometry().height, offset + textHeight(lines)) + std::abs(shift.y);
        const SkIRect area = SkRect::MakeLTRB(SkScalar((painter.offset().x - margin) * scale),
                                              SkScalar((painter.offset().y - margin) * scale),
                                              SkScalar((painter.offset().x + right + margin) * scale),
                                              SkScalar((painter.offset().y + bottom + margin) * scale))
                                 .roundOut();
        const SkSurfaceProps properties = canvas->getBaseProps();
        const sk_sp<SkSurface> raster = SkSurfaces::Raster(
            SkImageInfo::MakeN32Premul(area.width(), area.height()), &properties);
        if (raster) {
            raster->getCanvas()->clear(SK_ColorTRANSPARENT);
            raster->getCanvas()->translate(SkScalar(-area.x()), SkScalar(-area.y()));
            paintText(raster->getCanvas());
            canvas->drawImage(raster->makeImageSnapshot(), SkScalar(area.x()), SkScalar(area.y()));
        }
    } else {
        paintText(canvas);
    }
    if (faded)
        canvas->restore();
}

void Label::invalidate()
{
    m_paragraph.reset();
    m_elided.reset();
    m_wrapped.reset();
    m_wrappedWidth = -1;
    requestLayout();
}

std::unique_ptr<textlayout::Paragraph> Label::build(const std::string* text, const std::vector<size_t>* breaks) const
{
    const TextContext& context = Application::instance()->text();
    const Theme& theme = Theme::current();
    const Color color = m_color.value_or(theme.text);
    textlayout::TextStyle style = context.style(m_fontSize, m_weight, color, m_family);
    for (const auto& [feature, value] : m_features)
        style.addFontFeature(SkString(feature.c_str()), value);

    textlayout::ParagraphStyle paragraphStyle;
    paragraphStyle.setTextStyle(style);
    // Widths stay as Qt measures them, not rounded to hundredths.
    paragraphStyle.setApplyRoundingHack(false);
    // Aligned when painted, by Qt's advances.
    paragraphStyle.setTextAlign(textlayout::TextAlign::kLeft);
    if (!m_wrap) {
        paragraphStyle.setMaxLines(1);
        if (m_elide == Elide::Right)
            paragraphStyle.setEllipsis(SkString(kEllipsis));
    } else if (m_maximumLineCount > 0) {
        paragraphStyle.setMaxLines(size_t(m_maximumLineCount));
        paragraphStyle.setEllipsis(SkString(kEllipsis));
    }

    auto builder = textlayout::ParagraphBuilder::make(paragraphStyle, context.fontCollection(), context.unicode());
    // The part of the text from the offset, with the newlines that fall in it.
    size_t nextBreak = 0;
    const auto withBreaks = [&](const std::string& part, size_t offset) {
        if (!breaks)
            return part;
        std::string out;
        size_t from = 0;
        for (; nextBreak < breaks->size() && (*breaks)[nextBreak] < offset + part.size(); ++nextBreak) {
            const size_t at = (*breaks)[nextBreak] - offset;
            out.append(part, from, at - from);
            out += '\n';
            from = at;
        }
        out.append(part, from, std::string::npos);
        return out;
    };
    if (text || m_spans.empty()) {
        const std::string content = withBreaks(text ? *text : m_text, 0);
        builder->pushStyle(style);
        builder->addText(content.data(), content.size());
        builder->pop();
    } else {
        size_t offset = 0;
        for (const Span& span : m_spans) {
            textlayout::TextStyle spanStyle = style;
            if (span.weight > 0)
                spanStyle.setFontStyle(
                    SkFontStyle(span.weight, SkFontStyle::kNormal_Width, SkFontStyle::kUpright_Slant));
            const std::string content = withBreaks(span.text, offset);
            builder->pushStyle(spanStyle);
            builder->addText(content.data(), content.size());
            builder->pop();
            offset += span.text.size();
        }
    }
    return builder->Build();
}

textlayout::Paragraph* Label::paragraph() const
{
    const bool enabled = isEnabled();
    if (m_paragraph && m_paragraphEnabled == enabled && m_paragraphTheme == Theme::generation())
        return m_paragraph.get();

    m_paragraph = build(nullptr);
    m_paragraphEnabled = enabled;
    m_paragraphTheme = Theme::generation();
    m_layoutWidth = -1;
    m_naturalWidth = -1;
    m_elided.reset();
    m_wrapped.reset();
    m_wrappedWidth = -1;
    return m_paragraph.get();
}

// The code point boundaries of the text.
static std::vector<size_t> codePointStarts(const std::string& text)
{
    std::vector<size_t> starts;
    for (size_t i = 0; i < text.size(); ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80)
            starts.push_back(i);
    }
    starts.push_back(text.size());
    return starts;
}

textlayout::Paragraph* Label::wrappedParagraph(double width) const
{
    textlayout::Paragraph* text = paragraph();
    const bool round = roundsAdvances();
    if (m_wrappedWidth == width && m_wrappedRounded == round)
        return m_wrapped ? m_wrapped.get() : text;
    if (m_layoutWidth != kUnboundedWidth) {
        text->layout(SkScalar(kUnboundedWidth));
        m_layoutWidth = kUnboundedWidth;
    }
    std::string content = m_text;
    if (!m_spans.empty()) {
        content.clear();
        for (const Span& span : m_spans)
            content += span.text;
    }
    std::vector<size_t> starts = qtLineStarts(text, content, width, QtWrap::WordOrAnywhere, round);
    std::optional<std::string> elided;
    if (m_maximumLineCount > 0 && starts.size() > size_t(m_maximumLineCount)) {
        // QQuickText elides the last line it shows, the rest of the text with it.
        const size_t last = starts[size_t(m_maximumLineCount) - 1];
        const std::string rest = content.substr(last);
        const std::vector<size_t> restStarts = codePointStarts(rest);
        size_t low = 0;
        size_t high = restStarts.size() - 1;
        while (low < high) {
            const size_t middle = (low + high + 1) / 2;
            const std::string candidate = rest.substr(0, restStarts[middle]) + kEllipsis;
            std::unique_ptr<textlayout::Paragraph> trial = build(&candidate);
            trial->layout(SkScalar(kUnboundedWidth));
            if (naturalWidth(trial.get(), round) <= width)
                low = middle;
            else
                high = middle - 1;
        }
        elided = content.substr(0, last) + rest.substr(0, restStarts[low]) + kEllipsis;
        starts.resize(size_t(m_maximumLineCount));
    }

    m_wrappedWidth = width;
    m_wrappedRounded = round;
    m_wrappedElided = elided.has_value();
    // A newline where each line starts, unless one already ends the line before.
    m_wrappedBreaks.clear();
    for (size_t i = 1; i < starts.size(); ++i) {
        if (content[starts[i] - 1] != '\n')
            m_wrappedBreaks.push_back(starts[i]);
    }
    if (m_wrappedBreaks.empty() && !elided) {
        m_wrapped.reset();
        return text;
    }
    m_wrapped = build(elided ? &*elided : nullptr, &m_wrappedBreaks);
    m_wrapped->layout(SkScalar(kUnboundedWidth));
    return m_wrapped.get();
}

textlayout::Paragraph* Label::layoutParagraph(double width) const
{
    if (m_wrap && width < kUnboundedWidth)
        return wrappedParagraph(width);
    textlayout::Paragraph* text = paragraph();
    const double extra = width < kUnboundedWidth ? slack() + 1.0 / 64 : 0;
    if (width != m_layoutWidth) {
        // Breaks and elides where Qt's advances do.
        text->layout(SkScalar(width + extra));
        m_layoutWidth = width;
    }
    const bool round = roundsAdvances();
    if (m_elide != Elide::Middle || m_wrap || !m_spans.empty() || m_naturalWidth <= width)
        return text;

    if (m_elided && m_elidedWidth == width)
        return m_elided.get();

    // The most code points kept around the ellipsis, half each side, that fit.
    const std::vector<size_t> starts = codePointStarts(m_text);
    const size_t count = starts.size() - 1;
    const auto elided = [&](size_t kept) {
        const size_t head = (kept + 1) / 2;
        const size_t tail = kept / 2;
        return m_text.substr(0, starts[head]) + kEllipsis + m_text.substr(starts[count - tail]);
    };
    size_t low = 0;
    size_t high = count;
    std::unique_ptr<textlayout::Paragraph> best;
    while (low < high) {
        const size_t middle = (low + high + 1) / 2;
        const std::string candidate = elided(middle);
        std::unique_ptr<textlayout::Paragraph> trial = build(&candidate);
        trial->layout(SkScalar(kUnboundedWidth));
        if (naturalWidth(trial.get(), round) <= width) {
            low = middle;
            best = std::move(trial);
        } else {
            high = middle - 1;
        }
    }
    if (!best) {
        const std::string candidate = elided(low);
        best = build(&candidate);
    }
    best->layout(SkScalar(width + extra));
    m_elided = std::move(best);
    m_elidedWidth = width;
    return m_elided.get();
}

} // namespace cutefish::ui
