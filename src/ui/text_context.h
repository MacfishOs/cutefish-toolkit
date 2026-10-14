// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

#include "include/core/SkFont.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSurfaceProps.h"
#include "modules/skparagraph/include/TextStyle.h"

#include "ui/geometry.h"

class SkFontMgr;
class SkUnicode;

namespace skia::textlayout {
class FontCollection;
}

namespace cutefish::ui {

// Fonts and shaping shared by every window: the system font manager with
// fallback, and the rendering fontconfig asks for.
class TextContext
{
public:
    TextContext();
    ~TextContext();

    TextContext(const TextContext&) = delete;
    TextContext& operator=(const TextContext&) = delete;

    const sk_sp<skia::textlayout::FontCollection>& fontCollection() const { return m_fontCollection; }
    const sk_sp<SkUnicode>& unicode() const { return m_unicode; }

    const std::string& family() const { return m_family; }
    // BCP 47, for picking the Han variant among fallback fonts.
    const std::string& locale() const { return m_locale; }

    // The theme's family at a logical size, shaped and drawn at the scale.
    skia::textlayout::TextStyle style(double fontSize, double scale, int weight, Color color) const;

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
    SkFont::Edging m_edging = SkFont::Edging::kAntiAlias;
    SkFontHinting m_hinting = SkFontHinting::kSlight;
    bool m_subpixelPositioning = true;
    SkPixelGeometry m_pixelGeometry = kUnknown_SkPixelGeometry;
};

} // namespace cutefish::ui
