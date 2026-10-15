// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/icon.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "include/core/SkCanvas.h"

#include "ui/icon_loader.h"
#include "ui/painter.h"
#include "ui/theme.h"

namespace cutefish::ui {

// The square, in device pixels, an icon of the size takes in the view, as
// FishUI's IconItem places its raster: centred on the view, then snapped.
static Rect iconRect(const Painter& painter, const RectF& bounds, double size)
{
    const int side = std::max(1, int(std::lround(size * painter.scale())));
    const double extent = side / painter.scale();
    return painter.snappedRect({bounds.x + (bounds.width - extent) / 2, bounds.y + (bounds.height - extent) / 2,
                                extent, extent});
}

Icon::Icon(std::string name, double size)
    : m_name(std::move(name))
    , m_size(size)
{
}

void Icon::registerGlyph(const std::string& name, std::string svg)
{
    IconLoader::instance().registerGlyph(name, std::move(svg));
}

void Icon::setIconName(std::string name)
{
    m_name = std::move(name);
    requestPaint();
}

void Icon::setColor(std::optional<Color> color)
{
    m_color = color;
    requestPaint();
}

void Icon::setSize(double size)
{
    m_size = size;
    requestLayout();
}

void Icon::paint(Painter& painter)
{
    if (m_name.empty())
        return;
    const Theme& theme = Theme::current();
    const Color color = isEnabled() ? m_color.value_or(theme.text) : theme.disabledText;
    const Rect rect = iconRect(painter, localRect(), m_size);
    if (sk_sp<SkImage> image = IconLoader::instance().glyph(m_name, rect.width, color, painter.scale()))
        painter.drawImage(image.get(), rect.x, rect.y);
}

ThemeIcon::ThemeIcon(std::string name, double size)
    : m_name(std::move(name))
    , m_size(size)
{
}

void ThemeIcon::setIconName(std::string name)
{
    m_name = std::move(name);
    requestPaint();
}

void ThemeIcon::setSize(double size)
{
    m_size = size;
    requestLayout();
}

std::string ThemeIcon::path(double scale) const
{
    return IconLoader::instance().themeIconPath(m_name, int(m_size), scale);
}

void ThemeIcon::paint(Painter& painter)
{
    if (m_name.empty())
        return;
    const Rect rect = iconRect(painter, localRect(), m_size);
    if (m_provider) {
        // QIcon::pixmap(sourceSize × dpr) at the application's dpr, which is
        // the outputs' integer scale: the theme is searched at sourceSize × dpr
        // at that scale, for a pixmap that many times larger.
        const double scale = painter.scale();
        const int requested = int(std::lround(m_size * scale));
        const int iconScale = int(std::ceil(scale));
        const int lookupSize = requested;
        const int pixels = requested * iconScale;
        if (sk_sp<SkImage> image = IconLoader::instance().themeIcon(m_name, lookupSize, iconScale, pixels)) {
            SkPaint paint;
            paint.setAlphaf(float(painter.opacity()));
            painter.canvas()->drawImageRect(image, SkRect::MakeXYWH(SkScalar(rect.x), SkScalar(rect.y),
                                                                    SkScalar(rect.width), SkScalar(rect.height)),
                                            SkSamplingOptions(SkFilterMode::kLinear), &paint);
        }
        return;
    }
    // QIcon takes IconItem's size in whole logical pixels.
    if (sk_sp<SkImage> image = IconLoader::instance().themeIcon(m_name, int(m_size), painter.scale()))
        painter.drawImage(image.get(), rect.x, rect.y);
}

void ThemeIcon::setImageProviderLookup(bool provider)
{
    m_provider = provider;
    requestPaint();
}

} // namespace cutefish::ui
