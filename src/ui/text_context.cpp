// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/text_context.h"

#include <clocale>
#include <fontconfig/fontconfig.h>

#include "include/core/SkFontMgr.h"
#include "include/ports/SkFontMgr_fontconfig.h"
#include "include/ports/SkFontScanner_FreeType.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skunicode/include/SkUnicode_icu.h"

#include "ui/theme.h"

namespace cutefish::ui {

// "zh_CN.UTF-8" becomes "zh-CN"; "C" and "POSIX" name no language.
static std::string localeName()
{
    const char* name = std::setlocale(LC_CTYPE, nullptr);
    std::string locale = name ? name : "";
    locale = locale.substr(0, locale.find_first_of(".@"));
    if (locale == "C" || locale == "POSIX")
        return {};
    for (char& c : locale) {
        if (c == '_')
            c = '-';
    }
    return locale;
}

TextContext::TextContext()
    : m_locale(localeName())
{
    m_fontManager = SkFontMgr_New_FontConfig(nullptr, SkFontScanner_Make_FreeType());
    m_fontCollection = sk_make_sp<skia::textlayout::FontCollection>();
    m_fontCollection->setDefaultFontManager(m_fontManager);
    m_fontCollection->enableFontFallback();
    m_unicode = SkUnicodes::ICU::Make();
    readFontconfigDefaults();
}

TextContext::~TextContext() = default;

skia::textlayout::TextStyle TextContext::style(double fontSize, double scale, int weight,
                                               Color color) const
{
    const Theme& theme = Theme::current();
    const std::string& family = theme.fontFamily.empty() ? m_family : theme.fontFamily;
    skia::textlayout::TextStyle style;
    style.setFontFamilies({SkString(family.c_str())});
    style.setFontSize(SkScalar((fontSize > 0 ? fontSize : theme.fontSize()) * scale));
    style.setFontStyle(SkFontStyle(weight, SkFontStyle::kNormal_Width, SkFontStyle::kUpright_Slant));
    style.setColor(color.argb());
    style.setFontEdging(m_edging);
    style.setFontHinting(m_hinting);
    style.setSubpixel(m_subpixelPositioning);
    if (!m_locale.empty())
        style.setLocale(SkString(m_locale.c_str()));
    return style;
}

void TextContext::readFontconfigDefaults()
{
    FcPattern* pattern = FcNameParse(reinterpret_cast<const FcChar8*>("sans-serif"));
    if (!m_locale.empty())
        FcPatternAddString(pattern, FC_LANG, reinterpret_cast<const FcChar8*>(m_locale.c_str()));
    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result;
    FcPattern* match = FcFontMatch(nullptr, pattern, &result);
    FcPatternDestroy(pattern);
    if (!match)
        return;

    FcChar8* family = nullptr;
    if (FcPatternGetString(match, FC_FAMILY, 0, &family) == FcResultMatch)
        m_family = reinterpret_cast<const char*>(family);

    FcBool antialias = FcTrue;
    FcPatternGetBool(match, FC_ANTIALIAS, 0, &antialias);
    int rgba = FC_RGBA_UNKNOWN;
    FcPatternGetInteger(match, FC_RGBA, 0, &rgba);
    FcBool hinting = FcTrue;
    FcPatternGetBool(match, FC_HINTING, 0, &hinting);
    int hintStyle = FC_HINT_SLIGHT;
    FcPatternGetInteger(match, FC_HINT_STYLE, 0, &hintStyle);
    FcPatternDestroy(match);

    switch (rgba) {
    case FC_RGBA_RGB:
        m_pixelGeometry = kRGB_H_SkPixelGeometry;
        break;
    case FC_RGBA_BGR:
        m_pixelGeometry = kBGR_H_SkPixelGeometry;
        break;
    case FC_RGBA_VRGB:
        m_pixelGeometry = kRGB_V_SkPixelGeometry;
        break;
    case FC_RGBA_VBGR:
        m_pixelGeometry = kBGR_V_SkPixelGeometry;
        break;
    default:
        m_pixelGeometry = kUnknown_SkPixelGeometry;
        break;
    }

    if (!antialias)
        m_edging = SkFont::Edging::kAlias;
    else if (m_pixelGeometry != kUnknown_SkPixelGeometry)
        m_edging = SkFont::Edging::kSubpixelAntiAlias;
    else
        m_edging = SkFont::Edging::kAntiAlias;

    if (!hinting) {
        m_hinting = SkFontHinting::kNone;
    } else {
        switch (hintStyle) {
        case FC_HINT_NONE:
            m_hinting = SkFontHinting::kNone;
            break;
        case FC_HINT_SLIGHT:
            m_hinting = SkFontHinting::kSlight;
            break;
        case FC_HINT_MEDIUM:
            m_hinting = SkFontHinting::kNormal;
            break;
        default:
            m_hinting = SkFontHinting::kFull;
            break;
        }
    }
    // Horizontal hinting snaps glyph advances to whole pixels, which
    // fractional positions would undo.
    m_subpixelPositioning = m_hinting <= SkFontHinting::kSlight;
}

} // namespace cutefish::ui
