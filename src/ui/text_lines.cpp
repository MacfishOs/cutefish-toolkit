// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/text_lines.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkTypeface.h"
#include "modules/skparagraph/include/Paragraph.h"

#include "ui/application.h"
#include "ui/line_breaks.h"
#include "ui/text_context.h"

namespace textlayout = skia::textlayout;

namespace cutefish::ui {

std::vector<TextLineBox> textLineBoxes(textlayout::Paragraph* paragraph, const SkTypeface* typeface,
                                       double pixelSize, double scale)
{
    const TextContext& context = Application::instance()->text();
    // The typefaces each line's glyphs use: fallback fonts count, as each
    // QScriptItem takes the ascent of the engines its glyphs came from.
    const size_t count = std::max<size_t>(1, paragraph ? paragraph->lineNumber() : 0);
    std::vector<std::vector<sk_sp<SkTypeface>>> typefaces(count);
    if (paragraph) {
        paragraph->visit([&](int lineNumber, const textlayout::Paragraph::VisitorInfo* info) {
            if (!info || info->count == 0 || lineNumber < 0 || size_t(lineNumber) >= count)
                return;
            auto& used = typefaces[size_t(lineNumber)];
            const sk_sp<SkTypeface> face = info->font.refTypeface();
            if (face && std::none_of(used.begin(), used.end(), [&](const sk_sp<SkTypeface>& other) {
                    return other->uniqueID() == face->uniqueID();
                }))
                used.push_back(face);
        });
    }

    std::vector<TextLineBox> lines(count);
    double top = 0;
    for (size_t i = 0; i < count; ++i) {
        int64_t ascent = 0;
        int64_t descent = 0;
        const auto add = [&](const SkTypeface* face) {
            const QtFontMetrics metrics = context.metrics(face, pixelSize, scale);
            ascent = std::max(ascent, int64_t(std::lround(metrics.ascent * 64)));
            descent = std::max(descent, int64_t(std::lround(metrics.descent * 64)));
        };
        if (typefaces[i].empty())
            add(typeface);
        for (const sk_sp<SkTypeface>& face : typefaces[i])
            add(face.get());
        // QTextLine::height(): the 26.6 sum rounded up.
        const double height = ((ascent + descent + 63) & -64) / 64.0;
        lines[i] = {top, height, ascent / 64.0};
        top += height;
    }
    return lines;
}

double textHeight(const std::vector<TextLineBox>& lines)
{
    return lines.empty() ? 0 : lines.back().top + lines.back().height;
}

// Qt's 26.6 advances: truncated, as QFontEngineFT takes linearHoriAdvance.
static double truncated(double advance)
{
    return std::floor(advance * 64) / 64;
}

namespace {

// A run of glyphs of one font and colour on a line, at the paragraph's own
// positions.
struct GlyphRun
{
    SkFont font;
    std::vector<SkGlyphID> glyphs;
    // From the paragraph's left, one per glyph.
    std::vector<double> x;
    // Where each glyph's cluster starts in the text.
    std::vector<size_t> clusters;
    size_t utf8Offset = 0;
    double clipRight = 0;
};

// A line's runs in visual order, each glyph with the advance Qt gives it.
struct GlyphLine
{
    std::vector<GlyphRun> runs;
    std::vector<std::vector<double>> advances;
    // QScriptLine's textAdvance, which alignment uses, and textWidth, which
    // adds the last glyph's overhang and sizes the text.
    double advance = 0;
    double width = 0;
};

} // namespace

// QFontEngine::getGlyphBearings: a whole-pixel advance less the glyph's
// whole-pixel right edge.
static double rightBearing(const SkFont& runFont, SkGlyphID glyph, double advance)
{
    SkFont font = runFont;
    font.setSubpixel(false);
    font.setHinting(SkFontHinting::kNone);
    font.setLinearMetrics(true);
    SkRect bounds;
    font.getBounds({&glyph, 1}, {&bounds, 1}, nullptr);
    if (bounds.isEmpty())
        return 0;
    return std::floor(advance + 0.5) - std::ceil(bounds.right());
}

static std::vector<GlyphLine> glyphLines(textlayout::Paragraph* paragraph, bool round)
{
    std::vector<GlyphLine> lines(paragraph->lineNumber());
    paragraph->visit([&](int lineNumber, const textlayout::Paragraph::VisitorInfo* info) {
        if (!info || info->count == 0 || lineNumber < 0 || size_t(lineNumber) >= lines.size())
            return;
        GlyphRun run;
        run.font = info->font;
        run.glyphs.assign(info->glyphs, info->glyphs + info->count);
        for (int i = 0; i < info->count; ++i) {
            run.x.push_back(info->origin.fX + info->positions[i].fX);
            run.clusters.push_back(info->utf8Starts[i]);
        }
        run.utf8Offset = info->utf8Starts[0];
        // The run's clip, already from the paragraph's left.
        run.clipRight = info->advanceX;
        lines[size_t(lineNumber)].runs.push_back(std::move(run));
    });

    for (GlyphLine& line : lines) {
        std::sort(line.runs.begin(), line.runs.end(),
                  [](const GlyphRun& a, const GlyphRun& b) { return a.x.front() < b.x.front(); });
        // Each glyph reaches to the next one, the last by its own advance.
        for (size_t r = 0; r < line.runs.size(); ++r) {
            const GlyphRun& run = line.runs[r];
            std::vector<double> advances(run.glyphs.size());
            for (size_t i = 0; i < run.glyphs.size(); ++i) {
                double advance;
                if (i + 1 < run.glyphs.size()) {
                    advance = run.x[i + 1] - run.x[i];
                } else if (r + 1 < line.runs.size()) {
                    advance = line.runs[r + 1].x.front() - run.x[i];
                } else {
                    SkScalar width = 0;
                    run.font.getWidths({&run.glyphs[i], 1}, {&width, 1});
                    advance = width;
                }
                advance = truncated(advance);
                advances[i] = round ? std::floor(advance + 0.5) : advance;
                line.width += advances[i];
            }
            line.advances.push_back(std::move(advances));
        }
        line.advance = line.width;
        // QTextLine counts the last glyph's overhang past its advance.
        if (!line.runs.empty()) {
            const GlyphRun& last = line.runs.back();
            const double bearing = rightBearing(last.font, last.glyphs.back(), line.advances.back().back());
            if (bearing < 0)
                line.width -= bearing;
        }
    }
    return lines;
}

double naturalWidth(textlayout::Paragraph* paragraph, bool roundAdvances)
{
    double widest = 0;
    for (const GlyphLine& line : glyphLines(paragraph, roundAdvances))
        widest = std::max(widest, line.width);
    return widest;
}

namespace {

// A glyph cluster in text order, with what QTextLine::layout_helper asks of
// the characters it starts at.
struct Cluster
{
    size_t start = 0;
    double advance = 0;
    SkFont font;
    SkGlyphID lastGlyph = 0;
    double lastAdvance = 0;
    bool hasGlyph = false;
    bool space = false;
    bool lineBreak = false;
    bool separator = false;
};

constexpr double kBearingNotCalculated = 1;

} // namespace

static bool isSeparator(const std::string& text, size_t offset)
{
    if (text[offset] == '\n')
        return true;
    // U+2028 and U+2029.
    return offset + 2 < text.size() && static_cast<unsigned char>(text[offset]) == 0xE2
            && static_cast<unsigned char>(text[offset + 1]) == 0x80
            && (static_cast<unsigned char>(text[offset + 2]) == 0xA8
                || static_cast<unsigned char>(text[offset + 2]) == 0xA9);
}

static std::vector<Cluster> clustersOf(textlayout::Paragraph* paragraph, const std::string& text, bool round)
{
    std::vector<Cluster> clusters;
    for (const GlyphLine& line : glyphLines(paragraph, round)) {
        for (size_t r = 0; r < line.runs.size(); ++r) {
            const GlyphRun& run = line.runs[r];
            for (size_t i = 0; i < run.glyphs.size(); ++i) {
                const double advance = line.advances[r][i];
                if (clusters.empty() || clusters.back().start != run.clusters[i])
                    clusters.push_back({run.clusters[i]});
                Cluster& cluster = clusters.back();
                cluster.advance += advance;
                cluster.font = run.font;
                cluster.lastGlyph = run.glyphs[i];
                cluster.lastAdvance = advance;
                cluster.hasGlyph = true;
            }
        }
    }
    // Separators end lines whether or not they have glyphs of their own.
    for (size_t i = 0; i < text.size(); ++i) {
        if (isSeparator(text, i))
            clusters.push_back({i});
    }
    std::stable_sort(clusters.begin(), clusters.end(),
                     [](const Cluster& a, const Cluster& b) { return a.start < b.start; });
    std::vector<Cluster> merged;
    for (Cluster& cluster : clusters) {
        if (!merged.empty() && merged.back().start == cluster.start) {
            if (cluster.hasGlyph)
                merged.back() = cluster;
            continue;
        }
        merged.push_back(cluster);
    }

    // Qt's attributes, at the UTF-16 position of each cluster.
    std::u16string utf16;
    std::vector<size_t> utf16At(text.size() + 1, 0);
    for (size_t i = 0; i < text.size();) {
        utf16At[i] = utf16.size();
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        const size_t length = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
        uint32_t c = length == 1 ? lead : lead & (0xFF >> (length + 1));
        for (size_t k = 1; k < length && i + k < text.size(); ++k)
            c = (c << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
        if (c >= 0x10000) {
            utf16.push_back(char16_t(0xD800 + ((c - 0x10000) >> 10)));
            utf16.push_back(char16_t(0xDC00 + ((c - 0x10000) & 0x3FF)));
        } else {
            utf16.push_back(char16_t(c));
        }
        i += length;
    }
    utf16At[text.size()] = utf16.size();
    const QtCharAttributes attributes = qtCharAttributes(utf16);
    for (Cluster& cluster : merged) {
        const size_t position = utf16At[std::min(cluster.start, text.size())];
        cluster.separator = isSeparator(text, cluster.start);
        cluster.space = !cluster.separator && attributes.breakableSpace[position];
        cluster.lineBreak = attributes.lineBreak[position];
        if (cluster.separator)
            cluster.advance = 0;
    }
    return merged;
}

// QTextLine::layout_helper on clusters, from the cluster that starts the
// line: the cluster the next line starts at and the line's textWidth.
static std::pair<size_t, double> layoutLine(const std::vector<Cluster>& clusters, size_t from, double width,
                                            bool manualWrap, bool breakany)
{
    const size_t count = clusters.size();
    double lineWidth = 0;
    size_t lineLength = 0;
    double tmpWidth = 0;
    size_t tmpLength = 0;
    double spaceWidth = 0;
    size_t spaceLength = 0;
    double rightBearing = kBearingNotCalculated;
    bool whiteSpaceOrObject = true;
    ptrdiff_t previousGlyph = -1;

    const auto bearingOf = [&clusters](ptrdiff_t index) {
        if (index < 0 || !clusters[size_t(index)].hasGlyph)
            return 0.0;
        const Cluster& cluster = clusters[size_t(index)];
        return std::min(ui::rightBearing(cluster.font, cluster.lastGlyph, cluster.lastAdvance), 0.0);
    };
    const auto negativeBearing = [&rightBearing] {
        return rightBearing == kBearingNotCalculated ? 0.0 : -rightBearing;
    };
    const auto newWidth = [&] { return lineWidth + tmpWidth + spaceWidth + negativeBearing(); };
    // LineBreakHelper::checkFullOtherwiseExtend.
    const auto full = [&] {
        if (lineLength && !manualWrap && newWidth() > width)
            return true;
        lineWidth += tmpWidth + spaceWidth;
        lineLength += tmpLength + spaceLength;
        tmpWidth = spaceWidth = 0;
        tmpLength = spaceLength = 0;
        return false;
    };

    size_t k = from;
    for (;;) {
        if (k >= count) {
            full();
            break;
        }
        rightBearing = kBearingNotCalculated;
        const Cluster& cluster = clusters[k];
        if (cluster.separator) {
            whiteSpaceOrObject = true;
            ++tmpLength;
            ++k;
            rightBearing = bearingOf(previousGlyph);
            lineWidth += tmpWidth;
            lineLength += tmpLength;
            tmpWidth = 0;
            tmpLength = 0;
            break;
        }
        if (cluster.space) {
            if (k > 0 && !clusters[k - 1].space)
                previousGlyph = ptrdiff_t(k) - 1;
            whiteSpaceOrObject = true;
            while (k < count && clusters[k].space) {
                spaceWidth += clusters[k].advance;
                ++spaceLength;
                ++k;
            }
            continue;
        }
        if (!manualWrap && spaceWidth > width)
            break;
        whiteSpaceOrObject = false;
        if (k == 0 || previousGlyph < 0 || !clusters[k - 1].space)
            previousGlyph = ptrdiff_t(k) - 1;
        bool boundary = false;
        do {
            tmpWidth += clusters[k].advance;
            ++tmpLength;
            ++k;
            if (k >= count || clusters[k].space || clusters[k].lineBreak || clusters[k].separator) {
                boundary = true;
                break;
            }
        } while (!breakany);
        if (boundary || breakany) {
            // Qt skips this while the font's worst overhang fits; the result is the same.
            const double previousBearing = rightBearing;
            rightBearing = bearingOf(ptrdiff_t(k) - 1);
            if (full()) {
                rightBearing = previousBearing != kBearingNotCalculated ? previousBearing : bearingOf(previousGlyph);
                break;
            }
        }
        previousGlyph = ptrdiff_t(k) - 1;
    }

    if (rightBearing == kBearingNotCalculated && !whiteSpaceOrObject && k > 0)
        rightBearing = bearingOf(ptrdiff_t(k) - 1);
    double textWidth = lineWidth + negativeBearing();
    if (lineLength == 0) {
        textWidth += tmpWidth;
        lineLength += tmpLength;
    }
    return {from + lineLength + spaceLength, textWidth};
}

std::vector<size_t> qtLineStarts(textlayout::Paragraph* unbounded, const std::string& text, double width, QtWrap wrap,
                                 bool roundAdvances)
{
    std::vector<size_t> starts = {0};
    const std::vector<Cluster> clusters = clustersOf(unbounded, text, roundAdvances);
    const bool manualWrap = wrap == QtWrap::None;
    size_t from = 0;
    while (from < clusters.size()) {
        auto [next, textWidth] = layoutLine(clusters, from, width, manualWrap, wrap == QtWrap::Anywhere);
        // A word wider than the line starts it and breaks anywhere.
        if (wrap == QtWrap::WordOrAnywhere && textWidth > width)
            next = layoutLine(clusters, from, width, manualWrap, true).first;
        next = std::max(next, from + 1);
        if (next >= clusters.size()) {
            // A separator at the very end leaves an empty last line.
            if (clusters.back().separator)
                starts.push_back(text.size());
            break;
        }
        starts.push_back(clusters[next].start);
        from = next;
    }
    return starts;
}

void paintTextLines(SkCanvas* canvas, textlayout::Paragraph* paragraph, const std::vector<TextLineBox>& lines,
                    PointF offset, double width, double align, bool roundAdvances,
                    const std::function<SkColor(size_t utf8Offset)>& color)
{
    const std::vector<GlyphLine> glyphs = glyphLines(paragraph, roundAdvances);
    for (size_t l = 0; l < glyphs.size() && l < lines.size(); ++l) {
        const GlyphLine& line = glyphs[l];
        if (line.runs.empty())
            continue;
        // QTextEngine::alignLine: by the line's advance, without the overhang.
        double pen = line.runs.front().x.front() + (width - line.advance) * align;
        const SkScalar baseline = SkScalar(offset.y + lines[l].top + lines[l].ascent);
        for (size_t r = 0; r < line.runs.size(); ++r) {
            const GlyphRun& run = line.runs[r];
            const double runStart = pen;
            SkTextBlobBuilder builder;
            const auto& buffer = builder.allocRunPos(run.font, int(run.glyphs.size()));
            std::copy(run.glyphs.begin(), run.glyphs.end(), buffer.glyphs);
            SkPoint* points = buffer.points();
            for (size_t i = 0; i < run.glyphs.size(); ++i) {
                points[i] = {SkScalar(pen - runStart), 0};
                pen += line.advances[r][i];
            }
            const sk_sp<SkTextBlob> blob = builder.make();
            if (!blob)
                continue;
            SkPaint paint;
            paint.setAntiAlias(true);
            paint.setColor(color(run.utf8Offset));
            canvas->save();
            // Runs end where the paragraph clips them, as at an ellipsis.
            const double clipRight = run.clipRight + (runStart - run.x.front());
            canvas->clipRect(SkRect::MakeLTRB(-1e6f, -1e6f, SkScalar(offset.x + clipRight), 1e6f));
            canvas->drawTextBlob(blob, SkScalar(offset.x + runStart), baseline, paint);
            canvas->restore();
        }
    }
}

static size_t utf8Offset(const std::string& text, size_t utf16)
{
    size_t units = 0;
    size_t i = 0;
    for (; i < text.size(); ++i) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if ((byte & 0xc0) == 0x80)
            continue;
        if (units >= utf16)
            break;
        units += byte >= 0xf0 ? 2 : 1;
    }
    return i;
}

TextGeometry textGeometry(textlayout::Paragraph* paragraph, const std::string& text, double width, double align,
                          bool roundAdvances)
{
    TextGeometry geometry;
    std::vector<textlayout::LineMetrics> metrics;
    paragraph->getLineMetrics(metrics);
    const std::vector<GlyphLine> glyphs = glyphLines(paragraph, roundAdvances);
    for (size_t l = 0; l < metrics.size(); ++l) {
        TextGeometry::Line line;
        line.start = utf8Offset(text, metrics[l].fStartIndex);
        line.end = utf8Offset(text, metrics[l].fEndIndex);
        // A line's start excludes the spaces a soft break left; keep them.
        if (l > 0)
            line.start = std::min(line.start, geometry.lines.back().end);
        double pen = 0;
        if (l < glyphs.size() && !glyphs[l].runs.empty()) {
            const GlyphLine& glyphLine = glyphs[l];
            pen = glyphLine.runs.front().x.front() + (width - glyphLine.advance) * align;
            for (size_t r = 0; r < glyphLine.runs.size(); ++r) {
                const GlyphRun& run = glyphLine.runs[r];
                for (size_t i = 0; i < run.glyphs.size(); ++i) {
                    if (line.stops.empty() || line.stops.back().first < run.clusters[i])
                        line.stops.emplace_back(run.clusters[i], pen);
                    pen += glyphLine.advances[r][i];
                }
            }
        } else {
            pen = width * align;
        }
        // Spaces at a soft break have no glyphs; they end where the text does.
        if (line.stops.empty() || line.stops.back().first < line.end)
            line.stops.emplace_back(line.end, pen);
        if (line.stops.front().first > line.start)
            line.stops.insert(line.stops.begin(), {line.start, line.stops.front().second});
        geometry.lines.push_back(std::move(line));
    }
    if (geometry.lines.empty())
        geometry.lines.push_back({0, 0, {{0, width * align}}});
    return geometry;
}

size_t TextGeometry::lineOf(size_t offset) const
{
    for (size_t l = 0; l < lines.size(); ++l) {
        // At a soft break the caret goes to the next line's start.
        if (offset < lines[l].end || (offset == lines[l].end && (l + 1 == lines.size() || lines[l + 1].start > offset)))
            return l;
    }
    return lines.empty() ? 0 : lines.size() - 1;
}

double TextGeometry::x(size_t offset) const
{
    const Line& line = lines[lineOf(offset)];
    double result = line.stops.front().second;
    for (const auto& [stop, x] : line.stops) {
        if (stop > offset)
            break;
        result = x;
    }
    return result;
}

size_t TextGeometry::offsetAt(size_t index, double x) const
{
    const Line& line = lines[std::min(index, lines.size() - 1)];
    const auto& stops = line.stops;
    for (size_t i = 0; i + 1 < stops.size(); ++i) {
        if (x < (stops[i].second + stops[i + 1].second) / 2)
            return stops[i].first;
    }
    return stops.back().first;
}

RectF tightBoundingRect(textlayout::Paragraph* paragraph, double pixelSize, double scale)
{
    const bool hinted = Application::instance()->text().hintsGlyphMetrics(scale);
    double left = std::numeric_limits<double>::max();
    double top = std::numeric_limits<double>::max();
    double right = std::numeric_limits<double>::lowest();
    double bottom = std::numeric_limits<double>::lowest();
    paragraph->visit([&](int lineNumber, const textlayout::Paragraph::VisitorInfo* info) {
        if (lineNumber != 0 || !info)
            return;
        // QFontEngineFT::boundingBox: the glyph's box on whole pixels at the
        // pixel size, hinted only when Qt hints; the pen follows the shaped
        // advances, in 26.6.
        SkFont font = info->font;
        font.setSize(SkScalar(pixelSize));
        font.setSubpixel(false);
        font.setHinting(hinted ? SkFontHinting::kFull : SkFontHinting::kNone);
        font.setLinearMetrics(!hinted);
        std::vector<SkRect> bounds(size_t(info->count));
        font.getBounds({info->glyphs, size_t(info->count)}, {bounds.data(), bounds.size()}, nullptr);
        for (int i = 0; i < info->count; ++i) {
            const SkRect& box = bounds[size_t(i)];
            if (box.isEmpty())
                continue;
            const double pen = std::floor((info->origin.fX + info->positions[i].fX) * 64) / 64;
            const double x = pen + std::floor(box.left());
            const double width = std::ceil(box.right()) - std::floor(box.left());
            const double y = std::floor(box.top());
            const double height = std::ceil(box.bottom()) - std::floor(box.top());
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, std::ceil(x) + width);
            bottom = std::max(bottom, y + height);
        }
    });
    if (left > right)
        return {};
    return {left, top, right - left, bottom - top};
}

} // namespace cutefish::ui
