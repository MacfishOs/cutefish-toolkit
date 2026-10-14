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

// The square, in device pixels, an icon of the size takes in the view: whole
// pixels, centred on the view's own device rect.
static Rect iconRect(const Painter& painter, const RectF& bounds, double size)
{
    const Rect box = painter.deviceRect(bounds);
    const int side = std::max(1, int(std::lround(size * painter.scale())));
    return {box.x + (box.width - side) / 2, box.y + (box.height - side) / 2, side, side};
}

Icon::Icon(std::string name, double size)
    : m_name(std::move(name))
    , m_size(size)
{
}

void Icon::setName(std::string name)
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
    if (sk_sp<SkImage> image = IconLoader::instance().glyph(m_name, rect.width, color))
        painter.drawImage(image.get(), rect.x, rect.y);
}

ThemeIcon::ThemeIcon(std::string name, double size)
    : m_name(std::move(name))
    , m_size(size)
{
}

void ThemeIcon::setName(std::string name)
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
    return IconLoader::instance().themeIconPath(m_name, int(std::lround(m_size * scale)));
}

void ThemeIcon::paint(Painter& painter)
{
    if (m_name.empty())
        return;
    const Rect rect = iconRect(painter, localRect(), m_size);
    if (sk_sp<SkImage> image = IconLoader::instance().themeIcon(m_name, rect.width))
        painter.drawImage(image.get(), rect.x, rect.y);
}

} // namespace cutefish::ui
