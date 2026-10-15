// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <vector>

#include "ui/geometry.h"

namespace cutefish::ui {

// An outline in logical coordinates, relative to the view that draws it, for
// curves and charts. Unlike boxes, it is drawn antialiased and not snapped
// to the device grid.
class Path
{
public:
    void moveTo(PointF point);
    void lineTo(PointF point);
    void quadTo(PointF control, PointF end);
    void cubicTo(PointF control1, PointF control2, PointF end);
    void close();

    bool isEmpty() const { return m_verbs.empty(); }
    void clear();

private:
    friend class Painter;

    enum class Verb : uint8_t {
        Move,
        Line,
        Quad,
        Cubic,
        Close,
    };

    std::vector<Verb> m_verbs;
    std::vector<PointF> m_points;
};

struct GradientStop
{
    // 0 at the start, 1 at the end.
    double position = 0;
    Color color;
};

// Colours along a line, in the same logical coordinates as what is filled.
struct LinearGradient
{
    PointF start;
    PointF end;
    std::vector<GradientStop> stops;

    // From the top colour at the rect's top to the bottom one at its bottom.
    static LinearGradient vertical(const RectF& rect, Color top, Color bottom)
    {
        return {{rect.x, rect.y}, {rect.x, rect.bottom()}, {{0, top}, {1, bottom}}};
    }
};

enum class LineCap {
    Butt,
    Round,
    Square,
};

enum class LineJoin {
    Miter,
    Round,
    Bevel,
};

} // namespace cutefish::ui
