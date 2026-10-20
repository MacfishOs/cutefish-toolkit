// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ui/window_header.h"

#include <algorithm>
#include <cmath>

#include "include/core/SkImage.h"
#include "ui/application.h"
#include "ui/header_button.h"
#include "ui/icon_loader.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/window.h"

namespace cutefish::ui {

static constexpr double kButton = 46;
static constexpr double kButtonSpacing = 1;

namespace {

class TrafficLightButton : public HeaderButton
{
public:
    TrafficLightButton(const std::string& artwork, Color plateColor)
        : HeaderButton(artwork, kButton)
        , m_plateColor(plateColor)
    {
    }

    void paint(Painter& painter) override
    {
        const Window* window = this->window();
        const bool active = window && window->isActive();

        // draw the circle
        const double radius = 7.0; // logical pixels (14px diameter)
        const double size = radius * 2.0;
        const Rect icon = painter.snappedRect({std::round((localRect().width - size) / 2),
                                              std::round((localRect().height - size) / 2), size, size});

        const bool pressed = m_pressed || m_checked;
        const double opacity = active || m_hovered || pressed ? 1.0 : 0.35;
        Color color = m_plateColor;
        if (pressed) color = color.darker(1.2);

        painter.fillDeviceRoundedRect({icon.x, icon.y, icon.width, icon.height}, icon.width / 2.0, color.withAlpha(opacity));

        // if hovered, draw icon (glyph)
        if (m_hovered || pressed) {
            const std::string artworkName = currentArtwork();
            if (artworkName.empty()) return;
            // Draw the inner glyph in dark
            const Color ink(0, 0, 0, 150);
            sk_sp<SkImage> image = IconLoader::instance().glyph("window-" + artworkName, icon.width, ink);
            painter.save();
            painter.drawImage(image.get(), icon.x, icon.y);
            painter.restore();
        }
    }

protected:
    Color m_plateColor;
};

class CloseTrafficLightButton final : public TrafficLightButton
{
public:
    CloseTrafficLightButton()
        : TrafficLightButton("close", Color::rgb(0xFF5F56))
    {
    }
};

class MinimizeTrafficLightButton final : public TrafficLightButton
{
public:
    MinimizeTrafficLightButton()
        : TrafficLightButton("minimize", Color::rgb(0xFFBD2E))
    {
    }
};

class MaximizeTrafficLightButton final : public TrafficLightButton
{
public:
    MaximizeTrafficLightButton()
        : TrafficLightButton("maximize", Color::rgb(0x27C93F))
    {
    }

protected:
    std::string currentArtwork() const override
    {
        const Window* window = this->window();
        return window && window->states().maximized ? "restore" : "maximize";
    }
};

} // namespace

WindowHeader::WindowHeader(double height)
    : m_height(height)
    , m_minimize(addChild<MinimizeTrafficLightButton>())
    , m_maximize(addChild<MaximizeTrafficLightButton>())
    , m_close(addChild<CloseTrafficLightButton>())
{
    m_minimize->onClicked = [this] {
        if (Window* window = this->window())
            window->minimize();
    };
    m_maximize->onClicked = [this] {
        if (Window* window = this->window())
            window->setMaximized(!window->states().maximized);
    };
    m_close->onClicked = [this] {
        Window* window = this->window();
        if (window && window->onCloseRequested)
            window->onCloseRequested();
        else if (window && !window->isPopup())
            Application::instance()->quit();
    };
}

void WindowHeader::setContentView(std::unique_ptr<View> view)
{
    if (m_content)
        takeChild(m_content);
    m_content = view.get();
    addChild(std::move(view));
    // Under the buttons.
    raiseChild(m_minimize);
    raiseChild(m_maximize);
    raiseChild(m_close);
}

void WindowHeader::setMinimizeVisible(bool visible)
{
    m_minimize->setVisible(visible);
}

void WindowHeader::setMaximizeVisible(bool visible)
{
    m_maximize->setVisible(visible);
}

void WindowHeader::setCloseVisible(bool visible)
{
    m_close->setVisible(visible);
}

void WindowHeader::setButtonMargin(double margin)
{
    if (margin == m_buttonMargin)
        return;
    m_buttonMargin = margin;
    requestLayout();
}

void WindowHeader::setButtonsCentered(bool centered)
{
    if (centered == m_buttonsCentered)
        return;
    m_buttonsCentered = centered;
    requestLayout();
}

void WindowHeader::layout()
{
    const double h = geometry().height;
    const double buttonSize = std::min(h, 46.0);
    // A RowLayout centres on whole pixels.
    const double top = m_buttonsCentered ? std::floor((h - buttonSize) / 2) : m_buttonMargin;

    // Position buttons on the left side (macOS style)
    const double trafficLightWidth = 20; // 12px circle + 8px spacing
    double x = 12; // Start at 12px left margin
    // Order: Close, Minimize, Maximize
    for (HeaderButton* button : {m_close, m_minimize, m_maximize}) {
        if (!button->isVisible())
            continue;
        button->setGeometry({x, top, trafficLightWidth, buttonSize});
        x += trafficLightWidth;
    }
    // The content starts after the buttons
    if (m_content)
        m_content->setGeometry({x, 0, std::max(0.0, geometry().width - x), h});
}

void WindowHeader::pointerPress(const PointerEvent& event)
{
    Window* window = this->window();
    if (!window || event.button != MouseButton::Left || window->states().fullScreen)
        return;
    if (event.clickCount == 2) {
        window->setMaximized(!window->states().maximized);
        return;
    }
    window->startMove();
}

} // namespace cutefish::ui
