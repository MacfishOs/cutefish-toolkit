// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ui/view.h"

namespace skia::textlayout {
class Paragraph;
}

namespace cutefish::ui {

// Text laid out as FishUI's labels lay it out (Qt Quick Text with
// QtRendering): a whole logical pixel size, unhinted advances, lines as tall
// as Qt's QTextLine makes them. Glyphs are drawn at the device size.
class Label : public View
{
public:
    enum class TextAlignment {
        Left,
        Center,
        Right,
    };

    // Where a line too long for the label loses its text to an ellipsis.
    enum class Elide {
        // The line runs past the label.
        None,
        Right,
        Middle,
    };

    // A run of the text in its own weight or colour; 0 and unset take the
    // label's.
    struct Span
    {
        std::string text;
        int weight = 0;
        std::optional<Color> color;
    };

    explicit Label(std::string text = {});
    ~Label() override;

    const std::string& text() const { return m_text; }
    void setText(std::string text);
    // Logical pixels, rounded to whole ones as Qt rounds point sizes; 0
    // follows the theme.
    void setFontSize(double size);
    // OpenType features, such as {"tnum", 1} for figures of one width.
    void setFontFeatures(std::vector<std::pair<std::string, int>> features);
    void setWeight(int weight);
    // Empty follows the theme's.
    void setFontFamily(std::string family);
    // Unset follows the theme's text colour.
    void setColor(std::optional<Color> color);
    // Disabled, FishUI.Label fades to 0.6; Qt's Text and IconLabel, which
    // their controls colour, do not.
    void setFadesWhenDisabled(bool fades);
    void setWrap(bool wrap);
    // Unwrapped lines elide at the right unless set otherwise. Middle applies
    // to a single line of text without spans.
    void setElide(Elide elide);
    // With wrapping, at most this many lines, the last one elided; 0 for any.
    void setMaximumLineCount(int count);
    void setTextAlignment(TextAlignment alignment);
    void setVerticallyCentered(bool centered);
    // Replaces the text with runs of it, such as a bold month before a year.
    void setSpans(std::vector<Span> spans);

    // Whether the text as last laid out lost some of itself to an ellipsis,
    // e.g. to show it whole in a tool tip.
    bool isElided() const;

    // Qt's Text.baselineOffset: from the label's top to its first baseline,
    // in logical pixels, not snapped.
    double baselineOffset() const;
    // QFontMetricsF's ascent, descent and x-height for the label's font.
    double ascent() const;
    double descent() const;
    double xHeight() const;
    // The height of a tight "H", as FishUI.AlignedLabel takes it.
    double capHeight() const;
    // FishUI.AlignedLabel's capCenter: the middle of the capitals below the
    // label's top, snapped to the device grid.
    double capCenter() const;
    // FishUI.AlignedLabel's textBaseline: pads above the text by the
    // distance to y less the baseline offset, snapped to the device grid,
    // so the baseline lands near y below the label's top (at the top of its
    // layout, y in the parent). Negative turns it off.
    void setBaseline(double y);
    // QFontMetricsF::tightBoundingRect of the first line, relative to the
    // start of its baseline: whole pixels around the glyphs.
    RectF tightBoundingRect() const;

    // The unwrapped size.
    SizeF sizeHint() const override;
    double heightForWidth(double width) const override;

protected:
    void paint(Painter& painter) override;
    // The text moved by a logical offset, all of it in the colour when one is given.
    void paintText(Painter& painter, PointF shift, std::optional<Color> color);

private:
    void invalidate();
    double currentScale() const;
    double pixelSize() const;
    // With breaks, a newline goes before each of those UTF-8 offsets.
    std::unique_ptr<skia::textlayout::Paragraph> build(const std::string* text,
                                                       const std::vector<size_t>* breaks = nullptr) const;
    skia::textlayout::Paragraph* paragraph() const;
    // The paragraph as drawn at the logical width: the text, or the text
    // with its middle elided, or wrapped.
    skia::textlayout::Paragraph* layoutParagraph(double width) const;
    // In the lines QTextLine makes at the width, the last one shown elided
    // when there are more than the maximum.
    skia::textlayout::Paragraph* wrappedParagraph(double width) const;
    // Logical pixels from the label's top to the top of the text.
    double textOffset(double textHeight) const;
    // Whether Qt rounds advances at the window's scale.
    bool roundsAdvances() const;
    // How much wider the paragraph's own advances make the text than Qt's.
    double slack() const;
    double baselinePadding() const;
    double snapped(double value) const;

    std::string m_text;
    std::vector<Span> m_spans;
    std::vector<std::pair<std::string, int>> m_features;
    double m_fontSize = 0;
    int m_weight = 400;
    std::string m_family;
    bool m_fadesWhenDisabled = true;
    std::optional<Color> m_color;
    bool m_wrap = false;
    Elide m_elide = Elide::Right;
    int m_maximumLineCount = 0;
    TextAlignment m_alignment = TextAlignment::Left;
    bool m_verticallyCentered = false;
    double m_baseline = -1;

    mutable std::unique_ptr<skia::textlayout::Paragraph> m_paragraph;
    mutable bool m_paragraphEnabled = true;
    mutable uint64_t m_paragraphTheme = 0;
    mutable double m_layoutWidth = -1;
    mutable std::unique_ptr<skia::textlayout::Paragraph> m_elided;
    mutable double m_elidedWidth = -1;
    // Wrapped: the text with a newline where each line Qt made starts.
    mutable std::unique_ptr<skia::textlayout::Paragraph> m_wrapped;
    mutable std::vector<size_t> m_wrappedBreaks;
    mutable double m_wrappedWidth = -1;
    mutable bool m_wrappedRounded = false;
    mutable bool m_wrappedElided = false;
    // Measured unbounded, for the rounding they were measured with.
    mutable double m_naturalWidth = -1;
    mutable double m_intrinsicWidth = 0;
    mutable bool m_naturalRounded = false;
};

} // namespace cutefish::ui
