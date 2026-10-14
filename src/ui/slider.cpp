// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/slider.h"

#include <algorithm>
#include <cmath>

#include "ui/keys.h"
#include "ui/painter.h"
#include "ui/theme.h"

namespace cutefish::ui {

static constexpr double kTrack = 4;
static constexpr double kHandle = 22;
// FishUI's DropShadow: radius 10, 21 samples, four pixels down.
static constexpr double kShadowSigma = 5;
static constexpr double kShadowOffset = 4;

Slider::Slider() = default;

void Slider::setRange(double from, double to)
{
    m_from = from;
    m_to = to;
    setValue(m_value);
}

void Slider::setStepSize(double step)
{
    m_step = std::max(0.0, step);
    requestLayout();
}

void Slider::setValue(double value)
{
    value = snapped(value);
    if (value == m_value)
        return;
    m_value = value;
    requestPaint();
}

double Slider::snapped(double value) const
{
    const double low = std::min(m_from, m_to);
    const double high = std::max(m_from, m_to);
    value = std::clamp(value, low, high);
    if (m_step > 0)
        value = std::clamp(m_from + std::round((value - m_from) / m_step) * m_step, low, high);
    return value;
}

double Slider::position() const
{
    return m_to == m_from ? 0 : (m_value - m_from) / (m_to - m_from);
}

RectF Slider::visualRect() const
{
    // The handle's shadow, wherever the handle is.
    const double reach = Painter::shadowReach(kShadowSigma);
    return localRect().adjusted(-reach, std::min(0.0, kShadowOffset - reach), reach, kShadowOffset + reach);
}

SizeF Slider::sizeHint() const
{
    return {200, m_step > 0 ? 32.0 : 27.0};
}

RectF Slider::handleRect() const
{
    const RectF bounds = localRect();
    const double travel = std::max(0.0, bounds.width - kHandle);
    return {travel * position(), (bounds.height - kHandle) / 2, kHandle, kHandle};
}

void Slider::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const RectF bounds = localRect();
    const RectF handle = handleRect();
    const double centreY = bounds.height / 2;
    // The track runs between the handle's centres at both ends, its round
    // caps reaching a little past them.
    const double start = kHandle / 2 - kTrack / 2;
    const double length = std::max(0.0, bounds.width - kHandle) + kTrack;

    painter.save();
    if (!isEnabled())
        painter.multiplyOpacity(0.4);
    painter.fillRoundedRect({start, centreY - kTrack / 2, length, kTrack}, kTrack / 2,
                            theme.darkMode ? Color::rgb(0xFFFFFF).withAlpha(0.3)
                                           : Color::rgb(0x000000).withAlpha(0.1));
    if (position() > 0) {
        const double filled = std::max(0.0, bounds.width - kHandle) * position() + kTrack;
        painter.fillRoundedRect({start, centreY - kTrack / 2, filled, kTrack}, kTrack / 2,
                                theme.highlight);
    }
    if (m_step > 0 && m_to != m_from) {
        const int intervals = int(std::ceil(std::abs(m_to - m_from) / m_step));
        const double travel = std::max(0.0, bounds.width - kHandle);
        // Marks only while they stay apart.
        if (intervals > 0 && intervals <= int(travel / 6)) {
            const Color mark = theme.darkMode ? Color::rgb(0xFFFFFF).withAlpha(0.55)
                                              : Color::rgb(0x000000).withAlpha(0.35);
            for (int i = 0; i <= intervals; ++i) {
                const double x = kHandle / 2 + travel * std::min(1.0, double(i) / intervals);
                painter.drawVerticalLine(x, centreY + 12, 3, mark);
            }
        }
    }
    painter.restore();

    painter.save();
    if (!isEnabled())
        painter.multiplyOpacity(0.55);
    if (isEnabled())
        painter.drawShadow(handle, kHandle / 2, kShadowSigma,
                           Color::rgb(0).withAlpha(m_pressed ? 0.15 : 0.1), {0, kShadowOffset});
    painter.fillRoundedRect(handle, kHandle / 2, m_pressed ? Color::rgb(0xFAFAFA) : Color::rgb(0xFFFFFF));
    painter.strokeRoundedRect(handle, kHandle / 2, Color::rgb(0).withAlpha(0.04));
    painter.restore();
}

void Slider::moveTo(double value)
{
    value = snapped(value);
    if (value == m_value)
        return;
    m_value = value;
    requestPaint();
    if (onMoved)
        onMoved(m_value);
}

void Slider::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    const RectF handle = handleRect();
    m_pressed = true;
    // Grabbing the handle keeps it where it was taken; elsewhere it jumps there.
    m_grabOffset = handle.contains(event.position) ? event.position.x - (handle.x + kHandle / 2) : 0;
    pointerMove(event);
}

void Slider::pointerMove(const PointerEvent& event)
{
    if (!m_pressed)
        return;
    const double travel = geometry().width - kHandle;
    if (travel <= 0)
        return;
    const double fraction = std::clamp((event.position.x - m_grabOffset - kHandle / 2) / travel, 0.0, 1.0);
    moveTo(m_from + fraction * (m_to - m_from));
}

void Slider::pointerRelease(const PointerEvent&)
{
    m_pressed = false;
    requestPaint();
}

bool Slider::keyPress(const KeyEvent& event)
{
    const double step = m_step > 0 ? m_step : std::abs(m_to - m_from) / 100;
    const double direction = m_to >= m_from ? 1 : -1;
    switch (event.key) {
    case key::Left:
    case key::Down:
        moveTo(m_value - step * direction);
        return true;
    case key::Right:
    case key::Up:
        moveTo(m_value + step * direction);
        return true;
    case key::Home:
        moveTo(m_from);
        return true;
    case key::End:
        moveTo(m_to);
        return true;
    default:
        return false;
    }
}

} // namespace cutefish::ui
