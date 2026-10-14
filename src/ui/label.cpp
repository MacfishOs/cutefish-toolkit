// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/label.h"

#include <cmath>
#include <utility>

#include "include/core/SkCanvas.h"
#include "include/core/SkFontStyle.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"

#include "ui/application.h"
#include "ui/painter.h"
#include "ui/text_context.h"
#include "ui/theme.h"
#include "ui/window.h"

namespace textlayout = skia::textlayout;

namespace cutefish::ui {

// Wide enough that nothing wraps while measuring.
static constexpr double kUnboundedWidth = 1e6;

Label::Label(std::string text)
    : m_text(std::move(text))
{
}

Label::~Label() = default;

void Label::setText(std::string text)
{
    if (text == m_text)
        return;
    m_text = std::move(text);
    invalidate();
}

void Label::setFontSize(double size)
{
    if (size == m_fontSize)
        return;
    m_fontSize = size;
    invalidate();
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
    m_paragraph.reset();
    requestPaint();
}

void Label::setWrap(bool wrap)
{
    if (wrap == m_wrap)
        return;
    m_wrap = wrap;
    invalidate();
}

void Label::setTextAlignment(TextAlignment alignment)
{
    if (alignment == m_alignment)
        return;
    m_alignment = alignment;
    m_paragraph.reset();
    requestPaint();
}

void Label::setVerticallyCentered(bool centered)
{
    m_verticallyCentered = centered;
    requestPaint();
}

SizeF Label::sizeHint() const
{
    const double scale = currentScale();
    textlayout::Paragraph* text = paragraph(scale);
    // A wrapped paragraph's height depends on the width it was last laid out at.
    if (m_layoutWidth < 0 || m_wrap)
        layoutParagraph(kUnboundedWidth);
    return {std::ceil(text->getMaxIntrinsicWidth()) / scale, std::ceil(text->getHeight()) / scale};
}

double Label::heightForWidth(double width) const
{
    const double scale = currentScale();
    textlayout::Paragraph* text = paragraph(scale);
    layoutParagraph(std::round(width * scale));
    return std::ceil(text->getHeight()) / scale;
}

void Label::paint(Painter& painter)
{
    if (m_text.empty())
        return;
    const Rect box = painter.deviceRect(localRect());
    textlayout::Paragraph* text = paragraph(painter.scale());
    layoutParagraph(box.width);
    double y = box.y;
    if (m_verticallyCentered)
        y += std::round((box.height - text->getHeight()) / 2.0);
    SkCanvas* canvas = painter.canvas();
    // A layer only when faded: glyphs blend themselves otherwise.
    const bool faded = painter.opacity() < 1;
    if (faded)
        canvas->saveLayerAlphaf(nullptr, float(painter.opacity()));
    text->paint(canvas, SkScalar(box.x), SkScalar(y));
    if (faded)
        canvas->restore();
}

void Label::invalidate()
{
    m_paragraph.reset();
    requestLayout();
}

double Label::currentScale() const
{
    const Window* window = this->window();
    return window ? window->scale() : 1.0;
}

textlayout::Paragraph* Label::paragraph(double scale) const
{
    const bool enabled = isEnabled();
    if (m_paragraph && m_paragraphScale == scale && m_paragraphEnabled == enabled
        && m_paragraphTheme == Theme::generation())
        return m_paragraph.get();

    const TextContext& context = Application::instance()->text();
    const Theme& theme = Theme::current();
    const textlayout::TextStyle style = context.style(
        m_fontSize, scale, m_weight, enabled ? m_color.value_or(theme.text) : theme.disabledText);

    textlayout::ParagraphStyle paragraphStyle;
    paragraphStyle.setTextStyle(style);
    switch (m_alignment) {
    case TextAlignment::Left:
        paragraphStyle.setTextAlign(textlayout::TextAlign::kLeft);
        break;
    case TextAlignment::Center:
        paragraphStyle.setTextAlign(textlayout::TextAlign::kCenter);
        break;
    case TextAlignment::Right:
        paragraphStyle.setTextAlign(textlayout::TextAlign::kRight);
        break;
    }
    if (!m_wrap) {
        paragraphStyle.setMaxLines(1);
        paragraphStyle.setEllipsis(SkString("…"));
    }

    auto builder = textlayout::ParagraphBuilder::make(paragraphStyle, context.fontCollection(),
                                                      context.unicode());
    builder->pushStyle(style);
    builder->addText(m_text.data(), m_text.size());
    builder->pop();
    m_paragraph = builder->Build();
    m_paragraphScale = scale;
    m_paragraphEnabled = enabled;
    m_paragraphTheme = Theme::generation();
    m_layoutWidth = -1;
    return m_paragraph.get();
}

void Label::layoutParagraph(double deviceWidth) const
{
    if (deviceWidth == m_layoutWidth)
        return;
    m_paragraph->layout(SkScalar(deviceWidth));
    m_layoutWidth = deviceWidth;
}

} // namespace cutefish::ui
