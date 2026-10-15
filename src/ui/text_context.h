// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>
#include <mutex>
#include <string>

#include "include/core/SkFont.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSurfaceProps.h"
#include "modules/skparagraph/include/TextStyle.h"

#include "ui/geometry.h"

class SkFontMgr;
class SkTypeface;
class SkUnicode;

namespace skia::textlayout {
class FontCollection;
}

namespace cutefish::ui {

// What Qt's QFontMetricsF reports for a font, in logical pixels.
struct QtFontMetrics
{
    double ascent = 0;
    double descent = 0;
    double xHeight = 0;
    double capHeight = 0;
};

// Fonts and shaping shared by every window: the system font manager with
// fallback, and the rendering fontconfig asks for.
//
// Text lays out as FishUI's (Qt Quick, QtRendering) does, so both toolkits
// place it alike: at a whole logical pixel size, with unhinted advances,
// lines as tall as their ascent and descent rounded up. It is drawn at the
// device size.
class TextContext
{
public:
    TextContext();
    ~TextContext();

    TextContext(const TextContext&) = delete;
    TextContext& operator=(const TextContext&) = delete;

    const sk_sp<skia::textlayout::FontCollection>& fontCollection() const { return m_fontCollection; }
    const sk_sp<SkUnicode>& unicode() const { return m_unicode; }
    // Forgets fallback fonts and matched families, after fonts or the
    // appearance's font changed.
    void clearFontCaches();

    const std::string& family() const { return m_family; }
    // BCP 47, for picking the Han variant among fallback fonts.
    const std::string& locale() const { return m_locale; }

    // The pixel size Qt gives a logical size: whole pixels, as
    // QFontDatabasePrivate::load rounds point sizes.
    static double pixelSize(double logicalSize);

    // The theme's family at a logical size (0 for the theme's), rounded as
    // pixelSize() rounds it, in logical units.
    // A family given takes the theme's place, the theme's standing in when the
    // family is missing, as a QFont family does.
    skia::textlayout::TextStyle style(double fontSize, int weight, Color color,
                                      const std::string& family = {}) const;
    sk_sp<SkTypeface> typeface(int weight, const std::string& family = {}) const;

    // QFontEngine's height metrics for the typeface: from OS/2 or hhea, in
    // Qt's 26.6 fixed point. Qt rounds ascent and descent to whole pixels
    // when it hints, which it does only without scaling and with full or
    // medium hinting.
    QtFontMetrics metrics(const SkTypeface* typeface, double pixelSize, double scale) const;
    bool roundsMetrics(double scale) const;
    // Off, text lays out unrounded at every scale, scale 1 included, as Qt's
    // offscreen platform lays it out for the compositor's panels.
    void setHintedLayout(bool hinted) { m_hintedLayout = hinted; }
    // Glyph bounds and advances as Qt's FreeType engine reports them for
    // QFontMetricsF::tightBoundingRect.
    bool hintsGlyphMetrics(double scale) const { return roundsMetrics(scale); }

    // Surfaces blend glyph coverage linearly, as Qt Quick's text shaders
    // do, without Skia's contrast and gamma, which embolden light text on
    // dark backgrounds. Without a pixel geometry text is greyscale.
    SkSurfaceProps surfaceProps(bool subpixel = true) const
    {
        return SkSurfaceProps(0, subpixel ? m_pixelGeometry : kUnknown_SkPixelGeometry, 0, 1);
    }

    SkFont::Edging edging() const { return m_edging; }
    SkFontHinting hinting() const { return m_hinting; }
    bool subpixelPositioning() const { return m_subpixelPositioning; }
    SkPixelGeometry pixelGeometry() const { return m_pixelGeometry; }

private:
    void readFontconfigDefaults();

    sk_sp<SkFontMgr> m_fontManager;
    sk_sp<skia::textlayout::FontCollection> m_fontCollection;
    sk_sp<SkUnicode> m_unicode;
    std::string m_family = "sans-serif";
    std::string m_locale;
    struct Tables
    {
        bool valid = false;
        int unitsPerEm = 1000;
        double ascent = 0;
        double descent = 0;
        int xHeight = 0;
        int capHeight = 0;
        bool hasCapHeight = false;
    };
    const Tables& tables(const SkTypeface* typeface) const;

    mutable std::mutex m_tablesMutex;
    mutable std::map<uint32_t, Tables> m_tables;
    SkFont::Edging m_edging = SkFont::Edging::kAntiAlias;
    // What fontconfig asks for; layout uses unhinted advances regardless.
    SkFontHinting m_fontconfigHinting = SkFontHinting::kSlight;
    SkFontHinting m_hinting = SkFontHinting::kSlight;
    bool m_subpixelPositioning = true;
    bool m_hintedLayout = true;
    SkPixelGeometry m_pixelGeometry = kUnknown_SkPixelGeometry;
};

} // namespace cutefish::ui
