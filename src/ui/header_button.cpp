// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/header_button.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "include/core/SkImage.h"

#include "ui/icon_loader.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/window.h"

namespace cutefish::ui {

// The artwork is drawn on a 24-pixel grid; any other size would be soft.
static constexpr double kArtwork = 24;

HeaderButton::HeaderButton(std::string artwork, double size)
    : m_artwork(std::move(artwork))
    , m_size(size)
{
}

void HeaderButton::setArtwork(std::string artwork)
{
    m_artwork = std::move(artwork);
    requestPaint();
}

void HeaderButton::setGlyph(std::string name, double size)
{
    m_glyph = std::move(name);
    m_glyphSize = size;
    requestPaint();
}

void HeaderButton::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const Color tint = theme.darkMode ? Color::rgb(0xFFFFFF) : Color::rgb(0x323238);
    const Rect box = painter.deviceRect(localRect());
    const int artwork = int(std::lround(kArtwork * painter.scale()));

    // The plate keeps the artwork's parity inside the button, so both stay
    // centred on the same pixel.
    if (isEnabled() && (m_hovered || m_pressed)) {
        const double alpha = m_pressed ? (theme.darkMode ? 0.06 : 0.21) : (theme.darkMode ? 0.12 : 0.11);
        const int margin = int(std::lround(m_size * 0.1 * painter.scale()));
        int plate = std::min(box.width, box.height) - 2 * margin;
        if ((plate - artwork) % 2 != 0)
            plate -= 1;
        painter.fillDeviceRoundedRect({box.x + (box.width - plate) / 2,
                                       box.y + (box.height - plate) / 2, plate, plate},
                                      plate / 2.0, tint.withAlpha(alpha));
    }

    const std::string artworkName = currentArtwork();
    if (artworkName.empty() && m_glyph.empty())
        return;
    const Window* window = this->window();
    const double opacity = !isEnabled() ? 0.35 : (window && window->isActive()) ? 1.0 : 0.6;
    const int side = m_glyph.empty() ? artwork : int(std::lround(m_glyphSize * painter.scale()));
    sk_sp<SkImage> image = m_glyph.empty()
            ? IconLoader::instance().glyph("window-" + artworkName, side, tint)
            : IconLoader::instance().glyph(m_glyph, side, tint);
    painter.save();
    painter.multiplyOpacity(opacity);
    painter.drawImage(image.get(), box.x + (box.width - side) / 2, box.y + (box.height - side) / 2);
    painter.restore();
}

void HeaderButton::pointerEnter()
{
    m_hovered = true;
    requestPaint();
}

void HeaderButton::pointerLeave()
{
    m_hovered = false;
    requestPaint();
}

void HeaderButton::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    m_pressed = true;
    requestPaint();
}

void HeaderButton::pointerRelease(const PointerEvent& event)
{
    if (!m_pressed)
        return;
    m_pressed = false;
    requestPaint();
    if (localRect().contains(event.position) && onClicked)
        onClicked();
}

} // namespace cutefish::ui
