// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/window_header.h"

#include <algorithm>

#include "ui/application.h"
#include "ui/header_button.h"
#include "ui/window.h"

namespace cutefish::ui {

static constexpr double kButton = 32;
static constexpr double kButtonSpacing = 1;

namespace {

// Restore while the window is maximized, maximize otherwise.
class MaximizeButton final : public HeaderButton
{
public:
    MaximizeButton()
        : HeaderButton("maximize", kButton)
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
    , m_minimize(addChild<HeaderButton>("minimize", kButton))
    , m_maximize(addChild<MaximizeButton>())
    , m_close(addChild<HeaderButton>("close", kButton))
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
    m_buttonMargin = margin;
    requestLayout();
}

void WindowHeader::layout()
{
    double x = geometry().width - m_buttonMargin;
    for (HeaderButton* button : {m_close, m_maximize, m_minimize}) {
        if (!button->isVisible())
            continue;
        x -= kButton;
        button->setGeometry({x, m_buttonMargin, kButton, kButton});
        x -= kButtonSpacing;
    }
    if (m_content)
        m_content->setGeometry({0, 0, std::max(0.0, x), geometry().height});
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
