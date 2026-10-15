// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/text_context.h"

#include <algorithm>
#include <clocale>
#include <cmath>
#include <cstdint>
#include <fontconfig/fontconfig.h>
#include <vector>

#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkTypeface.h"
#include "include/ports/SkFontMgr_fontconfig.h"
#include "include/ports/SkFontScanner_FreeType.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skunicode/include/SkUnicode_icu.h"

#include "ui/line_break_unicode.h"
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
    // Lines wrap where Qt's do; ICU's rules keep "WireGuard .conf" together.
    m_unicode = makeQtLineBreakUnicode(SkUnicodes::ICU::Make());
    readFontconfigDefaults();
}

TextContext::~TextContext() = default;

double TextContext::pixelSize(double logicalSize)
{
    return std::round(std::floor(logicalSize * 100 + 0.5) / 100);
}

skia::textlayout::TextStyle TextContext::style(double fontSize, int weight, Color color,
                                               const std::string& family) const
{
    const Theme& theme = Theme::current();
    const std::string& themeFamily = theme.fontFamily.empty() ? m_family : theme.fontFamily;
    skia::textlayout::TextStyle style;
    if (family.empty())
        style.setFontFamilies({SkString(themeFamily.c_str())});
    else
        style.setFontFamilies({SkString(family.c_str()), SkString(themeFamily.c_str())});
    style.setFontSize(SkScalar(pixelSize(fontSize > 0 ? fontSize : theme.fontSize())));
    style.setFontStyle(SkFontStyle(weight, SkFontStyle::kNormal_Width, SkFontStyle::kUpright_Slant));
    style.setColor(color.argb());
    style.setFontEdging(m_edging);
    style.setFontHinting(m_hinting);
    style.setSubpixel(m_subpixelPositioning);
    if (!m_locale.empty())
        style.setLocale(SkString(m_locale.c_str()));
    return style;
}

sk_sp<SkTypeface> TextContext::typeface(int weight, const std::string& family) const
{
    const Theme& theme = Theme::current();
    const std::string& themeFamily = theme.fontFamily.empty() ? m_family : theme.fontFamily;
    const SkFontStyle style(weight, SkFontStyle::kNormal_Width, SkFontStyle::kUpright_Slant);
    std::vector<sk_sp<SkTypeface>> typefaces;
    if (!family.empty())
        typefaces = m_fontCollection->findTypefaces({SkString(family.c_str())}, style);
    if (typefaces.empty())
        typefaces = m_fontCollection->findTypefaces({SkString(themeFamily.c_str())}, style);
    if (typefaces.empty())
        typefaces = m_fontCollection->findTypefaces({SkString(m_family.c_str())}, SkFontStyle());
    return typefaces.empty() ? nullptr : typefaces.front();
}

static int16_t readInt16(const std::vector<uint8_t>& table, size_t offset)
{
    return int16_t(uint16_t(table[offset] << 8 | table[offset + 1]));
}

static uint16_t readUInt16(const std::vector<uint8_t>& table, size_t offset)
{
    return uint16_t(table[offset] << 8 | table[offset + 1]);
}

static std::vector<uint8_t> tableData(const SkTypeface* typeface, const char* tag)
{
    const SkFontTableTag value = SkSetFourByteTag(tag[0], tag[1], tag[2], tag[3]);
    std::vector<uint8_t> data(typeface->getTableSize(value));
    if (!data.empty())
        typeface->getTableData(value, 0, data.size(), data.data());
    return data;
}

// QFontEngine::processHheaTable and processOS2Table, in font units.
const TextContext::Tables& TextContext::tables(const SkTypeface* typeface) const
{
    std::lock_guard lock(m_tablesMutex);
    Tables& tables = m_tables[typeface->uniqueID()];
    if (tables.valid)
        return tables;
    tables.valid = true;
    tables.unitsPerEm = std::max(1, typeface->getUnitsPerEm());

    const std::vector<uint8_t> hhea = tableData(typeface, "hhea");
    if (hhea.size() >= 10 && (readInt16(hhea, 4) != 0 || readInt16(hhea, 6) != 0)) {
        tables.ascent = readInt16(hhea, 4);
        tables.descent = -readInt16(hhea, 6);
    }
    const std::vector<uint8_t> os2 = tableData(typeface, "OS/2");
    if (os2.size() >= 78) {
        const uint16_t selection = readUInt16(os2, 62);
        // USE_TYPO_METRICS.
        if (selection & 0x80) {
            if (readInt16(os2, 68) != 0 || readInt16(os2, 70) != 0) {
                tables.ascent = readInt16(os2, 68);
                tables.descent = -readInt16(os2, 70);
            }
        } else if (readUInt16(os2, 74) != 0 || readUInt16(os2, 76) != 0) {
            tables.ascent = readUInt16(os2, 74);
            tables.descent = readUInt16(os2, 76);
        }
        if (os2.size() >= 90) {
            tables.xHeight = readInt16(os2, 86);
            tables.capHeight = readInt16(os2, 88);
            tables.hasCapHeight = readUInt16(os2, 0) >= 2;
        }
    }
    if (tables.ascent == 0 && tables.descent == 0) {
        SkFont font(sk_ref_sp(const_cast<SkTypeface*>(typeface)), SkScalar(tables.unitsPerEm));
        SkFontMetrics metrics;
        font.getMetrics(&metrics);
        tables.ascent = -metrics.fAscent;
        tables.descent = metrics.fDescent;
    }
    return tables;
}

// Qt's QFixed: 26.6 fixed point, converted from reals by truncation and
// divided with rounding.
static int64_t fixedFromReal(double value)
{
    return int64_t(value * 64);
}

static int64_t fixedDivide(int64_t a, int64_t b)
{
    const bool negative = (a < 0) != (b < 0);
    const int64_t result = ((std::abs(a) << 6) + std::abs(b) / 2) / std::abs(b);
    return negative ? -result : result;
}

static int64_t fixedRound(int64_t value)
{
    return (value + 32) & -64;
}

// FreeType's FT_MulFix and FT_DivFix.
static int64_t mulFix(int64_t a, int64_t b)
{
    const bool negative = (a < 0) != (b < 0);
    const int64_t result = (std::abs(a) * std::abs(b) + 0x8000) >> 16;
    return negative ? -result : result;
}

static int64_t divFix(int64_t a, int64_t b)
{
    return ((a << 16) + b / 2) / b;
}

bool TextContext::roundsMetrics(double scale) const
{
    return scale == 1 && (m_fontconfigHinting == SkFontHinting::kNormal || m_fontconfigHinting == SkFontHinting::kFull);
}

QtFontMetrics TextContext::metrics(const SkTypeface* typeface, double pixelSize, double scale) const
{
    QtFontMetrics result;
    if (!typeface)
        return result;
    const Tables& t = tables(typeface);
    const int64_t unitsPerEm = int64_t(t.unitsPerEm) * 64;
    int64_t ascent = fixedDivide(fixedFromReal(t.ascent * pixelSize), unitsPerEm);
    int64_t descent = fixedDivide(fixedFromReal(t.descent * pixelSize), unitsPerEm);
    if (roundsMetrics(scale)) {
        ascent = fixedRound(ascent);
        descent = fixedRound(descent);
    }
    result.ascent = ascent / 64.0;
    result.descent = descent / 64.0;
    const int ppem = int(std::lround(pixelSize));
    if (t.xHeight > 0)
        result.xHeight = fixedDivide(int64_t(t.xHeight) * ppem * 64, unitsPerEm) / 64.0;
    if (t.hasCapHeight) {
        const int64_t yScale = divFix(int64_t(ppem) * 64, t.unitsPerEm);
        result.capHeight = mulFix(t.capHeight, yScale) / 64.0;
    }
    return result;
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
        m_fontconfigHinting = SkFontHinting::kNone;
    } else {
        switch (hintStyle) {
        case FC_HINT_NONE:
            m_fontconfigHinting = SkFontHinting::kNone;
            break;
        case FC_HINT_SLIGHT:
            m_fontconfigHinting = SkFontHinting::kSlight;
            break;
        case FC_HINT_MEDIUM:
            m_fontconfigHinting = SkFontHinting::kNormal;
            break;
        default:
            m_fontconfigHinting = SkFontHinting::kFull;
            break;
        }
    }
    // Horizontal hinting would round the advances, which Qt Quick's text
    // does not; vertical hinting keeps baselines and stems crisp.
    m_hinting = std::min(m_fontconfigHinting, SkFontHinting::kSlight);
    m_subpixelPositioning = true;
}

} // namespace cutefish::ui
