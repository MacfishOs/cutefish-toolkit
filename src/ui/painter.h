// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include "ui/geometry.h"

class SkCanvas;
class SkImage;

namespace cutefish::ui {

enum class Corners {
    Circular,
    // The compositor's window corners, for large panels; controls stay circular.
    Continuous,
};

// Paints in logical coordinates onto a canvas in device pixels. Every edge
// is rounded to the device grid at its absolute position, each on its own,
// so boxes that share a logical edge share a device edge and nothing
// straddles a pixel. Line widths are given in device pixels.
class Painter
{
public:
    Painter(SkCanvas* canvas, double scale);

    double scale() const { return m_scale; }
    SkCanvas* canvas() const { return m_canvas; }

    // Saves and restores the offset, the clip and the opacity.
    void save();
    void restore();
    void translate(PointF offset);
    PointF offset() const { return m_offset; }
    // Multiplies into what is drawn from here on.
    void multiplyOpacity(double opacity) { m_opacity *= opacity; }
    double opacity() const { return m_opacity; }
    // Intersects the clip with the rect, edges on device pixels.
    void clip(const RectF& rect);

    int toDevice(double logical) const;
    Rect deviceRect(const RectF& rect) const;
    // The whole device pixels a logical border takes, at least one, as FishUI's.
    int borderWidth(double logical) const;

    void fillRect(const RectF& rect, Color color);
    void fillRoundedRect(const RectF& rect, double radius, Color color,
                         Corners corners = Corners::Circular);
    // Exactly the device pixels given, for shapes centred by their own rules.
    void fillDeviceRoundedRect(const Rect& rect, double radiusPx, Color color);
    // A border inside the rect.
    void strokeRoundedRect(const RectF& rect, double radius, Color color, int widthPx = 1,
                           Corners corners = Corners::Circular);
    void fillEllipse(const RectF& rect, Color color);
    // How far past its rect a shadow of this blur shows.
    static double shadowReach(double sigma) { return 3 * sigma; }
    // A blurred copy of the rounded rect, sigma in logical pixels, under what follows.
    void drawShadow(const RectF& rect, double radius, double sigma, Color color, PointF offset = {});
    void drawHorizontalLine(double x, double y, double width, Color color, int widthPx = 1);
    void drawVerticalLine(double x, double y, double height, Color color, int widthPx = 1);
    // Antialiased and not snapped, for glyph-like strokes.
    void drawLine(PointF from, PointF to, Color color, double widthPx);
    // At whole device pixels.
    void drawImage(const SkImage* image, int x, int y);

private:
    struct State
    {
        PointF offset;
        double opacity;
    };

    SkCanvas* m_canvas;
    double m_scale;
    PointF m_offset;
    double m_opacity = 1;
    std::vector<State> m_saved;
};

} // namespace cutefish::ui
