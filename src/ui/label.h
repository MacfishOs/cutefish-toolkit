// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>

#include "ui/view.h"

namespace skia::textlayout {
class Paragraph;
}

namespace cutefish::ui {

// Text shaped and rasterised at the device size, so glyphs are hinted for
// the pixels they land on; its origin sits on a whole device pixel.
class Label : public View
{
public:
    enum class TextAlignment {
        Left,
        Center,
        Right,
    };

    explicit Label(std::string text = {});
    ~Label() override;

    const std::string& text() const { return m_text; }
    void setText(std::string text);
    // Logical pixels; 0 follows the theme.
    void setFontSize(double size);
    void setWeight(int weight);
    // Unset follows the theme's text colour. Disabled, the label takes the
    // theme's disabled text colour either way.
    void setColor(std::optional<Color> color);
    void setWrap(bool wrap);
    void setTextAlignment(TextAlignment alignment);
    void setVerticallyCentered(bool centered);

    // The unwrapped size.
    SizeF sizeHint() const override;
    double heightForWidth(double width) const override;

protected:
    void paint(Painter& painter) override;

private:
    void invalidate();
    double currentScale() const;
    skia::textlayout::Paragraph* paragraph(double scale) const;
    void layoutParagraph(double deviceWidth) const;

    std::string m_text;
    double m_fontSize = 0;
    int m_weight = 400;
    std::optional<Color> m_color;
    bool m_wrap = false;
    TextAlignment m_alignment = TextAlignment::Left;
    bool m_verticallyCentered = false;

    mutable std::unique_ptr<skia::textlayout::Paragraph> m_paragraph;
    mutable double m_paragraphScale = 0;
    mutable bool m_paragraphEnabled = true;
    mutable uint64_t m_paragraphTheme = 0;
    mutable double m_layoutWidth = -1;
};

} // namespace cutefish::ui
