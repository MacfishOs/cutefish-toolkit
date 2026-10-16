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
#include "ui/multi_effect_blur.h"
#include "ui/painter.h"
#include "ui/text_context.h"
#include "ui/scroll_view.h"
#include "ui/theme.h"

namespace cutefish::ui {

// Content below the bar feeds the blur, so its edge does not fade out.
static constexpr double kBlurPadding = 32;
static constexpr double kFadeDistance = 16;
static constexpr double kTintAlpha = 0.72;
// A blurMax of 32 takes four blur levels.
static constexpr int kBlurItems = 4;
static constexpr double kBlurMax = 32;

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
    const std::array<double, 6> weights = multiEffectBlurWeights(kBlurMax, 1);
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

    // The source: the content alone, from where the bar lies over the view, the
    // padding on the content's side.
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

            const SkBitmap bitmap = composite(texture, blurs, multiEffectBlurWeights(kBlurMax, 1), firstRow, lastRow);
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
