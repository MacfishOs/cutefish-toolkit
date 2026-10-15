// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "include/core/SkColor.h"

#include "ui/geometry.h"

class SkCanvas;
class SkTypeface;

namespace skia::textlayout {
class Paragraph;
}

namespace cutefish::ui {

// A line of a paragraph where Qt's QTextLine puts it, in logical pixels from
// the paragraph's top: the greatest ascent and descent of the fonts its
// glyphs use, fallback fonts included, and as tall as both rounded up.
struct TextLineBox
{
    double top = 0;
    double height = 0;
    double ascent = 0;
};

// The paragraph is laid out at the logical pixel size, in logical units.
std::vector<TextLineBox> textLineBoxes(skia::textlayout::Paragraph* paragraph, const SkTypeface* typeface,
                                       double pixelSize, double scale);
double textHeight(const std::vector<TextLineBox>& lines);

// The widest line's advance as Qt sums it: each glyph's design advance
// truncated to 26.6 fixed point, kerning included, and rounded to whole
// pixels when Qt hints (TextContext::roundsMetrics).
double naturalWidth(skia::textlayout::Paragraph* paragraph, bool roundAdvances);

// QTextOption's wrap modes.
enum class QtWrap {
    None,
    Word,
    Anywhere,
    WordOrAnywhere,
};

// Where QTextLine::layout_helper starts each line of the text at the width,
// on Qt's advances and break opportunities: UTF-8 offsets, the first 0. The
// paragraph holds the text laid out at an unbounded width.
std::vector<size_t> qtLineStarts(skia::textlayout::Paragraph* unbounded, const std::string& text, double width,
                                 QtWrap wrap, bool roundAdvances);

// Draws the paragraph's glyphs on the boxes' baselines and at Qt's
// truncated advances, offset in logical
// pixels on a canvas that maps logical pixels to device ones. The colour
// comes from the UTF-8 offset of each run.
// Lines start at the left of a paragraph laid out left aligned; `align` 0,
// 0.5 or 1 places each within the width as Qt aligns it.
void paintTextLines(SkCanvas* canvas, skia::textlayout::Paragraph* paragraph, const std::vector<TextLineBox>& lines,
                    PointF offset, double width, double align, bool roundAdvances,
                    const std::function<SkColor(size_t utf8Offset)>& color);

// Where a paragraph's text went, for carets, selections and hit tests:
// every line's UTF-8 range and the x of each cluster as drawn.
struct TextGeometry
{
    struct Line
    {
        size_t start = 0;
        // Before a line break.
        size_t end = 0;
        // Cluster starts and the line's end, by offset, with their x.
        std::vector<std::pair<size_t, double>> stops;
    };

    std::vector<Line> lines;

    size_t lineOf(size_t offset) const;
    // The caret's x at the offset, on its line.
    double x(size_t offset) const;
    // The cluster boundary nearest to x on the line.
    size_t offsetAt(size_t line, double x) const;
};

// The paragraph's text is `text`, in UTF-8.
TextGeometry textGeometry(skia::textlayout::Paragraph* paragraph, const std::string& text, double width,
                          double align, bool roundAdvances);

// QFontMetricsF::tightBoundingRect of the paragraph's first line, from the
// origin of its baseline: whole-pixel glyph boxes at Qt's advances.
RectF tightBoundingRect(skia::textlayout::Paragraph* paragraph, double pixelSize, double scale);

} // namespace cutefish::ui
