// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/multi_effect_blur.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "include/core/SkImageInfo.h"

namespace cutefish::ui {

Texture readTexture(const SkPixmap& pixmap)
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

// The texture size for an item: whole logical pixels, then the device
// pixel ratio.
int textureLength(double logical, double scale)
{
    return std::max(1, int(std::lround(std::ceil(logical - 1e-6) * scale)));
}

// One blur level: four taps a logical pixel of the item away, the
// second axis a third as far, averaged.
Texture blurPass(const Texture& source, double logicalWidth, double logicalHeight, double scale)
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

// The weights of the source and the blur levels, by how near each level lies to
// the blur's level of detail (calculateLod). The six levels are normalised
// together whatever the effect's blur level; levels it lacks get nothing.
std::array<double, 6> multiEffectBlurWeights(double blurMax, double blur)
{
    const double lod = std::sqrt(blur * blurMax / 64) * 1.2 - 0.2;
    std::array<double, 6> weights;
    double sum = 0;
    for (size_t i = 0; i < weights.size(); ++i) {
        weights[i] = std::clamp(1 - std::fabs(lod - (0.1 + 0.2 * double(i))) * 2, 0.0, 1.0);
        sum += weights[i];
    }
    for (double& weight : weights)
        weight /= sum;
    return weights;
}

int multiEffectBlurLevels(double blurMax)
{
    if (blurMax > 32)
        return 5;
    if (blurMax > 16)
        return 4;
    if (blurMax > 8)
        return 3;
    if (blurMax > 2)
        return 2;
    return 1;
}

// The effect's own pass over the item's quad: the source and the blur
// items, weighted, for the rows from first to last.
SkBitmap composite(const Texture& source, const std::vector<Texture>& blurs, const std::array<double, 6>& weights,
                   int firstRow, int lastRow)
{
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

} // namespace cutefish::ui
