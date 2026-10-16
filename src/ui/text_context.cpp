// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/text_context.h"

#include <algorithm>
#include <clocale>
#include <cmath>
#include <cstdint>
#include <map>
#include <mutex>
#include <strings.h>
#include <tuple>
#include <fontconfig/fontconfig.h>
#include <vector>

#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkStream.h"
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
namespace {

// The fonts of one family out of fontconfig's sorted set.
class FamilyStyleSet final : public SkFontStyleSet
{
public:
    explicit FamilyStyleSet(std::vector<sk_sp<SkTypeface>> typefaces)
        : m_typefaces(std::move(typefaces))
    {
    }

    int count() override { return int(m_typefaces.size()); }

    void getStyle(int index, SkFontStyle* style, SkString* name) override
    {
        if (style)
            *style = m_typefaces[size_t(index)]->fontStyle();
        if (name)
            name->reset();
    }

    sk_sp<SkTypeface> createTypeface(int index) override { return m_typefaces[size_t(index)]; }
    sk_sp<SkTypeface> matchStyle(const SkFontStyle& pattern) override { return matchStyleCSS3(pattern); }

private:
    std::vector<sk_sp<SkTypeface>> m_typefaces;
};

static bool sameFamily(const SkString& a, const char* b)
{
    return strcasecmp(a.c_str(), b) == 0;
}

// The system font manager with its character fallback remembered. SkParagraph
// keeps fallbacks only for one layout, so every layout asked fontconfig again
// for each character the requested fonts lack (about 2 ms per layout of a
// field of U+25CF); Chromium's FontCache keeps them across layouts.
class FallbackCachingFontMgr final : public SkFontMgr
{
public:
    explicit FallbackCachingFontMgr(sk_sp<SkFontMgr> fonts)
        : m_fonts(std::move(fonts))
    {
    }

    void clear()
    {
        std::lock_guard lock(m_mutex);
        m_fallbacks.clear();
    }

protected:
    int onCountFamilies() const override { return m_fonts->countFamilies(); }
    void onGetFamilyName(int index, SkString* name) const override { m_fonts->getFamilyName(index, name); }
    sk_sp<SkFontStyleSet> onCreateStyleSet(int index) const override { return m_fonts->createStyleSet(index); }
    // Fontconfig answers with every font, sorted; SkParagraph would then take
    // the best style of any family. Only the family's own fonts, or the best
    // match's family for an alias such as "monospace".
    sk_sp<SkFontStyleSet> onMatchFamily(const char family[]) const override
    {
        sk_sp<SkFontStyleSet> sorted = m_fonts->matchFamily(family);
        if (!sorted || !family)
            return sorted;
        std::vector<sk_sp<SkTypeface>> own;
        SkString first;
        std::vector<sk_sp<SkTypeface>> firstFamily;
        for (int i = 0; i < sorted->count(); ++i) {
            sk_sp<SkTypeface> typeface = sorted->createTypeface(i);
            if (!typeface)
                continue;
            SkString name;
            typeface->getFamilyName(&name);
            if (sameFamily(name, family))
                own.push_back(typeface);
            if (first.isEmpty())
                first = name;
            if (name.equals(first))
                firstFamily.push_back(typeface);
        }
        return sk_make_sp<FamilyStyleSet>(own.empty() ? std::move(firstFamily) : std::move(own));
    }

    sk_sp<SkTypeface> onMatchFamilyStyle(const char family[], const SkFontStyle& style) const override
    {
        return m_fonts->matchFamilyStyle(family, style);
    }

    // Everything the answer depends on is in the key; misses are kept too.
    sk_sp<SkTypeface> onMatchFamilyStyleCharacter(const char family[], const SkFontStyle& style,
                                                  const char* bcp47[], int bcp47Count,
                                                  SkUnichar character) const override
    {
        std::string languages;
        for (int i = 0; i < bcp47Count; ++i)
            languages.append(bcp47[i]).push_back(',');
        Key key{family ? family : "", style.weight(), style.width(), int(style.slant()), languages, character};
        std::lock_guard lock(m_mutex);
        const auto found = m_fallbacks.find(key);
        if (found != m_fallbacks.end())
            return found->second;
        sk_sp<SkTypeface> typeface = m_fonts->matchFamilyStyleCharacter(family, style, bcp47, bcp47Count,
                                                                        character);
        m_fallbacks.emplace(std::move(key), typeface);
        return typeface;
    }

    sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData> data, int index) const override
    {
        return m_fonts->makeFromData(std::move(data), index);
    }

    sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream, int index) const override
    {
        return m_fonts->makeFromStream(std::move(stream), index);
    }

    sk_sp<SkTypeface> onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset> stream,
                                           const SkFontArguments& arguments) const override
    {
        return m_fonts->makeFromStream(std::move(stream), arguments);
    }

    sk_sp<SkTypeface> onMakeFromFile(const char path[], int index) const override
    {
        return m_fonts->makeFromFile(path, index);
    }

    sk_sp<SkTypeface> onLegacyMakeTypeface(const char family[], SkFontStyle style) const override
    {
        return m_fonts->legacyMakeTypeface(family, style);
    }

private:
    using Key = std::tuple<std::string, int, int, int, std::string, SkUnichar>;

    sk_sp<SkFontMgr> m_fonts;
    mutable std::mutex m_mutex;
    mutable std::map<Key, sk_sp<SkTypeface>> m_fallbacks;
};

} // namespace

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
    m_fontManager = sk_make_sp<FallbackCachingFontMgr>(
        SkFontMgr_New_FontConfig(nullptr, SkFontScanner_Make_FreeType()));
    m_fontCollection = sk_make_sp<skia::textlayout::FontCollection>();
    m_fontCollection->setDefaultFontManager(m_fontManager);
    m_fontCollection->enableFontFallback();
    // Lines wrap by line_breaks.h, also after breakable white space; ICU's
    // rules keep "WireGuard .conf" together.
    m_unicode = makeLineBreakUnicode(SkUnicodes::ICU::Make());
    readFontconfigDefaults();
}

TextContext::~TextContext() = default;

void TextContext::clearFontCaches()
{
    static_cast<FallbackCachingFontMgr*>(m_fontManager.get())->clear();
    m_fontCollection->clearCaches();
}

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

// The hhea and OS/2 line metrics, in font units.
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

// 26.6 fixed point, converted from reals by truncation and divided with
// rounding.
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

bool TextContext::roundsMetrics(double scale, bool hintedLayout) const
{
    return scale == 1 && hintedLayout && (m_fontconfigHinting == SkFontHinting::kNormal || m_fontconfigHinting == SkFontHinting::kFull);
}

VerticalMetrics TextContext::metrics(const SkTypeface* typeface, double pixelSize, double scale,
                                   bool hintedLayout) const
{
    VerticalMetrics result;
    if (!typeface)
        return result;
    const Tables& t = tables(typeface);
    const int64_t unitsPerEm = int64_t(t.unitsPerEm) * 64;
    int64_t ascent = fixedDivide(fixedFromReal(t.ascent * pixelSize), unitsPerEm);
    int64_t descent = fixedDivide(fixedFromReal(t.descent * pixelSize), unitsPerEm);
    if (roundsMetrics(scale, hintedLayout)) {
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
    // Horizontal hinting would round the advances, which stay unhinted;
    // vertical hinting keeps baselines and stems crisp.
    m_hinting = std::min(m_fontconfigHinting, SkFontHinting::kSlight);
    m_subpixelPositioning = true;
}

} // namespace cutefish::ui
