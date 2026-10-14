// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/painter.h"

#include <algorithm>
#include <cmath>

#include "include/core/SkBlurTypes.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRRect.h"

#include "ui/rounded_path.h"

namespace cutefish::ui {

static SkRect toSkRect(const Rect& rect)
{
    return SkRect::MakeXYWH(rect.x, rect.y, rect.width, rect.height);
}

static SkPaint makePaint(Color color, double opacity, bool antialias)
{
    SkPaint paint;
    paint.setColor(color.withAlpha(color.a / 255.0 * opacity).argb());
    paint.setAntiAlias(antialias);
    return paint;
}

Painter::Painter(SkCanvas* canvas, double scale)
    : m_canvas(canvas)
    , m_scale(scale)
{
}

void Painter::save()
{
    m_saved.push_back({m_offset, m_opacity});
    m_canvas->save();
}

void Painter::restore()
{
    m_offset = m_saved.back().offset;
    m_opacity = m_saved.back().opacity;
    m_saved.pop_back();
    m_canvas->restore();
}

void Painter::clip(const RectF& rect)
{
    m_canvas->clipRect(toSkRect(deviceRect(rect)));
}

void Painter::translate(PointF offset)
{
    m_offset.x += offset.x;
    m_offset.y += offset.y;
}

int Painter::toDevice(double logical) const
{
    return int(std::lround(logical * m_scale));
}

Rect Painter::deviceRect(const RectF& rect) const
{
    const int left = toDevice(m_offset.x + rect.x);
    const int top = toDevice(m_offset.y + rect.y);
    const int right = toDevice(m_offset.x + rect.right());
    const int bottom = toDevice(m_offset.y + rect.bottom());
    return {left, top, right - left, bottom - top};
}

int Painter::borderWidth(double logical) const
{
    return std::max(1, int(std::lround(logical * m_scale)));
}

void Painter::fillRect(const RectF& rect, Color color)
{
    m_canvas->drawRect(toSkRect(deviceRect(rect)), makePaint(color, m_opacity, false));
}

void Painter::fillRoundedRect(const RectF& rect, double radius, Color color, Corners corners)
{
    const SkRect device = toSkRect(deviceRect(rect));
    if (corners == Corners::Continuous) {
        m_canvas->drawPath(roundedRectPath(device, radius * m_scale, true), makePaint(color, m_opacity, true));
        return;
    }
    const SkScalar r = SkScalar(radius * m_scale);
    m_canvas->drawRRect(SkRRect::MakeRectXY(device, r, r), makePaint(color, m_opacity, true));
}

void Painter::fillDeviceRoundedRect(const Rect& rect, double radiusPx, Color color)
{
    const SkScalar r = SkScalar(radiusPx);
    m_canvas->drawRRect(SkRRect::MakeRectXY(toSkRect(rect), r, r), makePaint(color, m_opacity, true));
}

void Painter::strokeRoundedRect(const RectF& rect, double radius, Color color, int widthPx,
                                Corners corners)
{
    const SkRect outerRect = toSkRect(deviceRect(rect));
    const SkScalar r = SkScalar(radius * m_scale);
    // Concentric: the inner edge's radius is the outer one less the width.
    const SkScalar innerRadius = std::max<SkScalar>(0, r - widthPx);
    const SkRect innerRect = outerRect.makeInset(SkScalar(widthPx), SkScalar(widthPx));
    if (corners == Corners::Continuous) {
        SkPathBuilder ring;
        ring.addPath(roundedRectPath(outerRect, r, true));
        ring.addPath(roundedRectPath(innerRect, innerRadius, true));
        ring.setFillType(SkPathFillType::kEvenOdd);
        m_canvas->drawPath(ring.detach(), makePaint(color, m_opacity, true));
        return;
    }
    const SkRRect outer = SkRRect::MakeRectXY(outerRect, r, r);
    const SkRRect inner = SkRRect::MakeRectXY(innerRect, innerRadius, innerRadius);
    m_canvas->drawDRRect(outer, inner, makePaint(color, m_opacity, true));
}

void Painter::fillEllipse(const RectF& rect, Color color)
{
    m_canvas->drawOval(toSkRect(deviceRect(rect)), makePaint(color, m_opacity, true));
}

void Painter::drawShadow(const RectF& rect, double radius, double sigma, Color color,
                         PointF offset)
{
    SkPaint paint = makePaint(color, m_opacity, true);
    paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, SkScalar(sigma * m_scale)));
    const RectF shifted = {rect.x + offset.x, rect.y + offset.y, rect.width, rect.height};
    const SkScalar r = SkScalar(radius * m_scale);
    m_canvas->drawRRect(SkRRect::MakeRectXY(toSkRect(deviceRect(shifted)), r, r), paint);
}

void Painter::drawHorizontalLine(double x, double y, double width, Color color, int widthPx)
{
    const int left = toDevice(m_offset.x + x);
    const int right = toDevice(m_offset.x + x + width);
    const int top = toDevice(m_offset.y + y);
    m_canvas->drawRect(toSkRect({left, top, right - left, widthPx}), makePaint(color, m_opacity, false));
}

void Painter::drawVerticalLine(double x, double y, double height, Color color, int widthPx)
{
    const int left = toDevice(m_offset.x + x);
    const int top = toDevice(m_offset.y + y);
    const int bottom = toDevice(m_offset.y + y + height);
    m_canvas->drawRect(toSkRect({left, top, widthPx, bottom - top}), makePaint(color, m_opacity, false));
}

void Painter::drawImage(const SkImage* image, int x, int y)
{
    if (!image)
        return;
    SkPaint paint;
    paint.setAlphaf(float(m_opacity));
    m_canvas->drawImage(image, SkScalar(x), SkScalar(y), SkSamplingOptions(), &paint);
}

void Painter::drawLine(PointF from, PointF to, Color color, double widthPx)
{
    SkPaint paint = makePaint(color, m_opacity, true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(SkScalar(widthPx));
    paint.setStrokeCap(SkPaint::kRound_Cap);
    m_canvas->drawLine(SkScalar((m_offset.x + from.x) * m_scale),
                       SkScalar((m_offset.y + from.y) * m_scale),
                       SkScalar((m_offset.x + to.x) * m_scale),
                       SkScalar((m_offset.y + to.y) * m_scale), paint);
}

} // namespace cutefish::ui
