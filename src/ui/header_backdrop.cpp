// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/header_backdrop.h"

#include <algorithm>
#include <cstdio>
#include <array>
#include <cmath>
#include <vector>

#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkRuntimeEffect.h"

#include "ui/application.h"
#include "ui/painter.h"
#include "ui/text_context.h"
#include "ui/scroll_view.h"
#include "ui/theme.h"

namespace cutefish::ui {

// Content below the bar feeds the blur, so its edge does not fade out.
static constexpr double kBlurPadding = 32;
static constexpr double kFadeDistance = 16;
static constexpr double kTintAlpha = 0.72;
// MultiEffect with blurMax 32 runs four blur items.
static constexpr int kBlurItems = 4;
static constexpr double kBlurMax = 32;

// Premultiplied RGBA in floats, sampled as a linearly filtered texture
// clamped at its edges.
struct Texture
{
    int width = 0;
    int height = 0;
    std::vector<float> pixels;

    // Texels already within the texture.
    void blend(int x0, int x1, float fx, int y0, int y1, float fy, float weight, float* out) const
    {
        const float* row0 = &pixels[size_t(y0) * size_t(width) * 4];
        const float* row1 = &pixels[size_t(y1) * size_t(width) * 4];
        const float* a = row0 + x0 * 4;
        const float* b = row0 + x1 * 4;
        const float* c = row1 + x0 * 4;
        const float* d = row1 + x1 * 4;
        for (int i = 0; i < 4; ++i) {
            const float top = a[i] + (b[i] - a[i]) * fx;
            const float bottom = c[i] + (d[i] - c[i]) * fx;
            out[i] += (top + (bottom - top) * fy) * weight;
        }
    }
};

// Where a linearly filtered texture of one length is sampled for each
// pixel of an output of another, both covering the same quad.
struct Taps
{
    std::vector<int> first;
    std::vector<int> second;
    std::vector<float> fraction;

    // offset in normalised texture coordinates.
    Taps(int output, int texture, double offset = 0)
    {
        for (int i = 0; i < output; ++i) {
            const double t = ((i + 0.5) / output + offset) * texture - 0.5;
            const int low = int(std::floor(t));
            first.push_back(std::clamp(low, 0, texture - 1));
            second.push_back(std::clamp(low + 1, 0, texture - 1));
            fraction.push_back(float(t - low));
        }
    }
};

static Texture readTexture(const SkPixmap& pixmap)
{
    Texture texture;
    texture.width = pixmap.width();
    texture.height = pixmap.height();
    texture.pixels.resize(size_t(texture.width) * size_t(texture.height) * 4);
    const bool bgra = pixmap.colorType() == kBGRA_8888_SkColorType;
    float* out = texture.pixels.data();
    for (int y = 0; y < texture.height; ++y) {
        const auto* row = static_cast<const uint8_t*>(pixmap.addr(0, y));
        for (int x = 0; x < texture.width; ++x, row += 4, out += 4) {
            out[0] = row[bgra ? 2 : 0] / 255.0f;
            out[1] = row[1] / 255.0f;
            out[2] = row[bgra ? 0 : 2] / 255.0f;
            out[3] = row[3] / 255.0f;
        }
    }
    return texture;
}

// QQuickShaderEffectSource's texture size for an item: whole logical
// pixels, then the device pixel ratio.
static int textureLength(double logical, double scale)
{
    return std::max(1, int(std::lround(std::ceil(logical - 1e-6) * scale)));
}

// MultiEffect's BlurItem: four taps a logical pixel of the item away, the
// second axis a third as far, averaged.
static Texture blurPass(const Texture& source, double logicalWidth, double logicalHeight, double scale)
{
    Texture result;
    result.width = textureLength(logicalWidth, scale);
    result.height = textureLength(logicalHeight, scale);
    result.pixels.assign(size_t(result.width) * size_t(result.height) * 4, 0.0f);
    const double ox = 1 / logicalWidth;
    const double oy = 1 / logicalHeight;
    const double dither = 0.33;
    // The four taps' offsets, as bluritems.vert places them.
    const double offsets[4][2] = {
        {ox, oy * dither},
        {ox * dither, -oy},
        {-ox * dither, oy},
        {-ox, -oy * dither},
    };
    std::vector<Taps> columns;
    std::vector<Taps> rows;
    for (const auto& offset : offsets) {
        columns.emplace_back(result.width, source.width, offset[0]);
        rows.emplace_back(result.height, source.height, offset[1]);
    }
    for (int y = 0; y < result.height; ++y) {
        float* out = &result.pixels[size_t(y) * size_t(result.width) * 4];
        for (int x = 0; x < result.width; ++x, out += 4) {
            for (size_t k = 0; k < 4; ++k) {
                source.blend(columns[k].first[size_t(x)], columns[k].second[size_t(x)], columns[k].fraction[size_t(x)],
                             rows[k].first[size_t(y)], rows[k].second[size_t(y)], rows[k].fraction[size_t(y)], 0.25f,
                             out);
            }
        }
    }
    return result;
}

// QQuickMultiEffectPrivate::getBlurWeights for blur 1: the source and the
// blur items, by how near each level lies to the blur's level of detail.
static std::array<double, kBlurItems + 1> blurWeights()
{
    const double lod = std::sqrt(1.0 * kBlurMax / 64) * 1.2 - 0.2;
    std::array<double, kBlurItems + 1> weights;
    double sum = 0;
    for (size_t i = 0; i < weights.size(); ++i) {
        weights[i] = std::clamp(1 - std::fabs(lod - (0.1 + 0.2 * double(i))) * 2, 0.0, 1.0);
        sum += weights[i];
    }
    // The sixth level only exists with blurMax over 32.
    sum += std::clamp(1 - std::fabs(lod - 1.1) * 2, 0.0, 1.0);
    for (double& weight : weights)
        weight /= sum;
    return weights;
}

// The effect's own pass over the item's quad: the source and the blur
// items, weighted, for the rows from first to last.
static SkBitmap composite(const Texture& source, const std::vector<Texture>& blurs, int firstRow, int lastRow)
{
    const std::array<double, kBlurItems + 1> weights = blurWeights();
    std::vector<const Texture*> textures = {&source};
    for (const Texture& blur : blurs)
        textures.push_back(&blur);
    std::vector<Taps> columns;
    std::vector<Taps> rows;
    for (const Texture* texture : textures) {
        columns.emplace_back(source.width, texture->width);
        rows.emplace_back(source.height, texture->height);
    }

    // Linear filtering is separable: each texture's rows widened to the
    // output first, then blended down the columns into the sum.
    const int width = source.width;
    const int height = std::max(1, lastRow - firstRow);
    std::vector<float> sum(size_t(width) * size_t(height) * 4, 0.0f);
    std::vector<float> widened;
    for (size_t i = 0; i < textures.size(); ++i) {
        if (weights[i] <= 0)
            continue;
        const Texture& texture = *textures[i];
        const Taps& column = columns[i];
        widened.assign(size_t(width) * size_t(texture.height) * 4, 0.0f);
        for (int ty = 0; ty < texture.height; ++ty) {
            const float* in = &texture.pixels[size_t(ty) * size_t(texture.width) * 4];
            float* out = &widened[size_t(ty) * size_t(width) * 4];
            for (int x = 0; x < width; ++x, out += 4) {
                const float* a = in + column.first[size_t(x)] * 4;
                const float* b = in + column.second[size_t(x)] * 4;
                const float f = column.fraction[size_t(x)];
                for (int c = 0; c < 4; ++c)
                    out[c] = a[c] + (b[c] - a[c]) * f;
            }
        }
        const float weight = float(weights[i]);
        for (int y = firstRow; y < lastRow; ++y) {
            const float* top = &widened[size_t(rows[i].first[size_t(y)]) * size_t(width) * 4];
            const float* bottom = &widened[size_t(rows[i].second[size_t(y)]) * size_t(width) * 4];
            const float f = rows[i].fraction[size_t(y)];
            float* out = &sum[size_t(y - firstRow) * size_t(width) * 4];
            for (int k = 0; k < width * 4; ++k)
                out[k] += (top[k] + (bottom[k] - top[k]) * f) * weight;
        }
    }

    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType));
    for (int y = 0; y < height; ++y) {
        auto* out = static_cast<uint8_t*>(bitmap.getAddr(0, y));
        const float* in = &sum[size_t(y) * size_t(width) * 4];
        for (int k = 0; k < width * 4; ++k)
            out[k] = uint8_t(std::clamp(in[k], 0.0f, 1.0f) * 255 + 0.5f);
    }
    bitmap.setImmutable();
    return bitmap;
}

// The same passes as runtime shaders, when the window draws on the GPU.
static const char* const kBlurItemShader = R"(
uniform shader source;
uniform float2 sourceSize;
uniform float2 outputSize;
uniform float2 offset;

half4 main(float2 p)
{
    float2 uv = p / outputSize;
    float dither = 0.33;
    return (source.eval((uv + float2(offset.x, offset.y * dither)) * sourceSize)
            + source.eval((uv + float2(offset.x * dither, -offset.y)) * sourceSize)
            + source.eval((uv + float2(-offset.x * dither, offset.y)) * sourceSize)
            + source.eval((uv + float2(-offset.x, -offset.y * dither)) * sourceSize)) / 4;
}
)";

static const char* const kCompositeShader = R"(
uniform shader blur1;
uniform shader blur2;
uniform shader blur3;
uniform shader blur4;
uniform float2 size1;
uniform float2 size2;
uniform float2 size3;
uniform float2 size4;
uniform float2 outputSize;
uniform float4 weights;

half4 main(float2 p)
{
    float2 uv = p / outputSize;
    return blur1.eval(uv * size1) * weights.x + blur2.eval(uv * size2) * weights.y
            + blur3.eval(uv * size3) * weights.z + blur4.eval(uv * size4) * weights.w;
}
)";

static sk_sp<SkRuntimeEffect> makeEffect(const char* code)
{
    SkRuntimeEffect::Result result = SkRuntimeEffect::MakeForShader(SkString(code));
    if (!result.effect)
        fprintf(stderr, "cutefish-toolkit: %s\n", result.errorText.c_str());
    return result.effect;
}

static sk_sp<SkShader> textureShader(const sk_sp<SkImage>& image)
{
    return image->makeShader(SkTileMode::kClamp, SkTileMode::kClamp, SkSamplingOptions(SkFilterMode::kLinear));
}

// The blurred source on the GPU: the blur items as textures, then the
// effect drawn at the device position for the rows from first to last.
static void drawBlurredOnGpu(SkCanvas* canvas, SkSurface* source, double width, double height, double scale,
                             SkPoint origin, int firstRow, int lastRow, float opacity)
{
    static const sk_sp<SkRuntimeEffect> blurItem = makeEffect(kBlurItemShader);
    static const sk_sp<SkRuntimeEffect> compositeEffect = makeEffect(kCompositeShader);
    if (!blurItem || !compositeEffect)
        return;
    sk_sp<SkImage> previous = source->makeImageSnapshot();
    std::vector<sk_sp<SkImage>> blurs;
    double itemWidth = std::ceil(width / 16) * 8;
    double itemHeight = std::ceil(height / 16) * 8;
    for (int i = 0; i < kBlurItems; ++i) {
        const int pixelWidth = textureLength(itemWidth, scale);
        const int pixelHeight = textureLength(itemHeight, scale);
        sk_sp<SkSurface> surface = source->makeSurface(pixelWidth, pixelHeight);
        if (!surface)
            return;
        SkRuntimeShaderBuilder builder(blurItem);
        builder.child("source") = textureShader(previous);
        builder.uniform("sourceSize") = SkV2{float(previous->width()), float(previous->height())};
        builder.uniform("outputSize") = SkV2{float(pixelWidth), float(pixelHeight)};
        builder.uniform("offset") = SkV2{float(1 / itemWidth), float(1 / itemHeight)};
        SkPaint paint;
        paint.setShader(builder.makeShader());
        paint.setBlendMode(SkBlendMode::kSrc);
        surface->getCanvas()->drawRect(SkRect::MakeIWH(pixelWidth, pixelHeight), paint);
        previous = surface->makeImageSnapshot();
        blurs.push_back(previous);
        itemWidth /= 2;
        itemHeight /= 2;
    }

    // The source's own weight is nil at blur 1.
    const std::array<double, kBlurItems + 1> weights = blurWeights();
    SkRuntimeShaderBuilder builder(compositeEffect);
    const char* const children[] = {"blur1", "blur2", "blur3", "blur4"};
    const char* const sizes[] = {"size1", "size2", "size3", "size4"};
    for (size_t i = 0; i < blurs.size(); ++i) {
        builder.child(children[i]) = textureShader(blurs[i]);
        builder.uniform(sizes[i]) = SkV2{float(blurs[i]->width()), float(blurs[i]->height())};
    }
    builder.uniform("outputSize") = SkV2{float(source->width()), float(source->height())};
    builder.uniform("weights") = SkV4{float(weights[1]), float(weights[2]), float(weights[3]), float(weights[4])};
    SkPaint paint;
    paint.setShader(builder.makeShader());
    paint.setAlphaf(opacity);
    canvas->save();
    canvas->translate(origin.fX, origin.fY);
    canvas->drawRect(SkRect::MakeLTRB(0, float(firstRow), float(source->width()), float(lastRow)), paint);
    canvas->restore();
}

HeaderBackdrop::HeaderBackdrop(ScrollView* view, Edge edge)
    : m_view(view)
    , m_edge(edge)
{
    m_scrolled = view->scrolled.connect([this] { requestPaint(); });
}

HeaderBackdrop::~HeaderBackdrop() = default;

void HeaderBackdrop::setSeparatorVisible(bool visible)
{
    m_separatorVisible = visible;
    requestPaint();
}

void HeaderBackdrop::setBaseColor(std::optional<Color> color)
{
    m_baseColor = color;
    requestPaint();
}

void HeaderBackdrop::setTintColor(std::optional<Color> color)
{
    m_tintColor = color;
    requestPaint();
}

double HeaderBackdrop::fade() const
{
    const double position = m_view->scrollPosition().y;
    const double end = m_view->contentSize().height - m_view->geometry().height;
    const double distance = m_edge == Edge::Bottom ? end - position : position;
    return std::clamp(distance / kFadeDistance, 0.0, 1.0);
}

void HeaderBackdrop::paint(Painter& painter)
{
    const double opacity = fade();
    View* content = m_view->contentView();
    if (opacity <= 0 || !content)
        return;
    const Theme& theme = Theme::current();
    const RectF bounds = localRect();
    const bool bottom = m_edge == Edge::Bottom;
    painter.save();
    painter.multiplyOpacity(opacity);
    painter.clip(bounds);

    // The view's content is still drawn beneath; an opaque base hides it.
    painter.fillRect(bounds, m_baseColor.value_or(theme.background));

    // ShaderEffectSource: the content alone, from where the bar lies over the
    // view, the padding on the content's side.
    const double scale = painter.scale();
    const double width = bounds.width;
    const double height = bounds.height + kBlurPadding;
    const PointF viewPosition = mapToWindow({}) - m_view->mapToWindow({});
    const SkImageInfo info = SkImageInfo::MakeN32Premul(textureLength(width, scale), textureLength(height, scale));
    // Greyscale text, as in any texture that starts transparent.
    const SkSurfaceProps properties = Application::instance()->text().surfaceProps(false);
    // Made by the window's canvas, so layered views draw from their textures.
    sk_sp<SkSurface> surface = painter.canvas()->makeSurface(info, &properties);
    if (!surface)
        surface = SkSurfaces::Raster(info, &properties);
    if (surface) {
        surface->getCanvas()->clear(SK_ColorTRANSPARENT);
        Painter source(surface->getCanvas(), scale);
        source.translate({-viewPosition.x, -(viewPosition.y - (bottom ? kBlurPadding : 0))});
        content->paintTree(source, nullptr);

        // Only the rows the clip leaves: the padding lies outside the bar.
        const int padding = info.height() - textureLength(bounds.height, scale);
        const int firstRow = bottom ? std::max(0, padding) : 0;
        const int lastRow = bottom ? info.height() : info.height() - std::max(0, padding);
        const PointF origin = painter.offset() + PointF{0, bottom ? -kBlurPadding : 0};
        const SkPoint deviceOrigin = SkPoint::Make(SkScalar(std::round(origin.x * scale)),
                                                   SkScalar(std::round(origin.y * scale)));
        SkBitmap pixels;
        SkPixmap pixmap;
        if (surface->recordingContext()) {
            drawBlurredOnGpu(painter.canvas(), surface.get(), width, height, scale, deviceOrigin, firstRow, lastRow,
                             float(painter.opacity()));
        } else if (pixels.tryAllocPixels(info) && surface->readPixels(pixels.pixmap(), 0, 0)
                   && pixels.peekPixels(&pixmap)) {
            const Texture texture = readTexture(pixmap);
            // The first blur item is half the source, in multiples of eight;
            // each next one half the one before.
            std::vector<Texture> blurs;
            double itemWidth = std::ceil(width / 16) * 8;
            double itemHeight = std::ceil(height / 16) * 8;
            for (int i = 0; i < kBlurItems; ++i) {
                blurs.push_back(blurPass(i == 0 ? texture : blurs.back(), itemWidth, itemHeight, scale));
                itemWidth /= 2;
                itemHeight /= 2;
            }

            const SkBitmap bitmap = composite(texture, blurs, firstRow, lastRow);
            SkPaint paint;
            paint.setAlphaf(float(painter.opacity()));
            painter.canvas()->drawImage(bitmap.asImage(), deviceOrigin.fX, deviceOrigin.fY + float(firstRow),
                                        SkSamplingOptions(), &paint);
        }
    }

    painter.fillRect(bounds, m_tintColor.value_or(theme.secondBackground).withAlpha(kTintAlpha));

    // A 0.5 px hairline; below 2x a device pixel with its alpha scaled to
    // the same weight.
    if (m_separatorVisible) {
        const double devicePixels = std::max(1.0, std::round(0.5 * scale));
        const double lineHeight = devicePixels / scale;
        const double alpha = (theme.darkMode ? 0.12 : 0.08) * 0.5 * scale / devicePixels;
        painter.drawRoundedRect({0, bottom ? 0 : bounds.height - lineHeight, bounds.width, lineHeight}, {0},
                                theme.text.withAlpha(alpha));
    }
    painter.restore();
}

} // namespace cutefish::ui
