// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "include/core/SkImage.h"
#include "include/core/SkRefCnt.h"

class SkCanvas;

namespace cutefish::ui {

// What a QML Image with layer.enabled and a FishUI.OpacityMask of a
// FishUI.RoundedRectangle filling it puts in the window, as Qt Quick renders
// it on the GPU: the image drawn into a layer texture of ceil(size) * dpr
// pixels, the mask's rounded rectangle snapped and drawn into one of its
// own, and the effect sampling both linearly where the item's quad covers
// pixel centres, at its exact position. Within two levels of Qt's output.
struct QuickLayerItem
{
    // The Image item in logical window coordinates, unsnapped.
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
    double scale = 1;
    enum class FillMode {
        Stretch,
        PreserveAspectFit,
        PreserveAspectCrop,
    } fillMode = FillMode::Stretch;
    bool smooth = true;
    bool mipmap = false;
    // The mask's FishUI.RoundedRectangle.
    double radius = 0;
    bool continuousCorners = false;
};

// The layer and the mask as textures; they do not depend on where the item is.
struct QuickLayerTextures
{
    sk_sp<SkImage> layer;
    sk_sp<SkImage> mask;
    double width = 0;
    double height = 0;
    double scale = 1;
};

QuickLayerTextures renderQuickLayerTextures(const SkImage& picture, const QuickLayerItem& item);
// The effect: both textures filtered where the quad of the item at x, y
// (logical) covers pixel centres, on a canvas in device pixels.
void drawQuickLayer(SkCanvas* canvas, const QuickLayerTextures& textures, double x, double y, float opacity);

} // namespace cutefish::ui
