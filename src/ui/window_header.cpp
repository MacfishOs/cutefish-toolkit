// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ui/window_header.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "base/event_loop.h"
#include "base/timer.h"
#include "include/core/SkImage.h"
#include "ui/application.h"
#include "ui/header_button.h"
#include "ui/icon_loader.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/popup_glass.h"
#include "ui/theme.h"
#include "ui/window.h"

namespace cutefish::ui {

static constexpr double kButton = 46;
static constexpr double kButtonSpacing = 1;

namespace {

enum class TileAction {
    None,
    LeftHalf,
    RightHalf,
    TopHalf,
    BottomHalf,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Zoom,
    FullScreen,
};

class SnapPopoverView;

struct HeaderState {
    std::unique_ptr<Window> popoverWindow;
    SnapPopoverView* popoverView = nullptr;
    std::unique_ptr<base::Timer> hoverTimer;
    std::unique_ptr<base::Timer> dismissTimer;
};

static std::unordered_map<const WindowHeader*, std::unique_ptr<HeaderState>> s_headerStates;

static HeaderState& getHeaderState(const WindowHeader* header)
{
    auto& state = s_headerStates[header];
    if (!state) {
        state = std::make_unique<HeaderState>();
        state->hoverTimer = std::make_unique<base::Timer>();
        state->hoverTimer->setSingleShot(true);
        state->hoverTimer->setInterval(std::chrono::milliseconds(350));

        state->dismissTimer = std::make_unique<base::Timer>();
        state->dismissTimer->setSingleShot(true);
        state->dismissTimer->setInterval(std::chrono::milliseconds(250));
    }
    return *state;
}

static void cancelDismissTimer(const WindowHeader* header)
{
    auto it = s_headerStates.find(header);
    if (it != s_headerStates.end() && it->second->dismissTimer) {
        it->second->dismissTimer->stop();
    }
}

static void startDismissTimer(const WindowHeader* header)
{
    auto it = s_headerStates.find(header);
    if (it != s_headerStates.end() && it->second->dismissTimer) {
        it->second->dismissTimer->start();
    }
}

static void removeHeaderState(const WindowHeader* header)
{
    auto it = s_headerStates.find(header);
    if (it != s_headerStates.end()) {
        if (it->second->hoverTimer) it->second->hoverTimer->stop();
        if (it->second->dismissTimer) it->second->dismissTimer->stop();
        if (it->second->popoverWindow) {
            std::shared_ptr<Window> win(std::move(it->second->popoverWindow));
            win->hide();
            Application::instance()->loop().post([win] {});
        }
        s_headerStates.erase(it);
    }
}

class SnapPopoverView final : public View
{
public:
    explicit SnapPopoverView(Window* targetWindow, WindowHeader* header)
        : m_target(targetWindow)
        , m_header(header)
    {
        setTracksHover(true);

        m_header1 = addChild<Label>("Move & Resize");
        m_header1->setFontSize(10.5);
        m_header1->setWeight(600);
        m_header1->setGeometry({14, 10, 208, 16});

        m_header2 = addChild<Label>("Quarters");
        m_header2->setFontSize(10.5);
        m_header2->setWeight(600);
        m_header2->setGeometry({14, 76, 208, 16});

        const bool isMax = targetWindow && targetWindow->states().maximized;
        m_zoomLabel = addChild<Label>(isMax ? "Restore Size" : "Fill Screen");
        m_zoomLabel->setFontSize(12.0);
        m_zoomLabel->setWeight(500);
        m_zoomLabel->setGeometry({42, 148, 184, 20});

        const bool isFs = targetWindow && targetWindow->states().fullScreen;
        m_fullScreenLabel = addChild<Label>(isFs ? "Exit Full Screen" : "Enter Full Screen");
        m_fullScreenLabel->setFontSize(12.0);
        m_fullScreenLabel->setWeight(500);
        m_fullScreenLabel->setGeometry({42, 180, 184, 20});

        updateColors();
    }

    bool acceptsPointer() const override { return true; }

    void updateColors()
    {
        const bool dark = Theme::current().darkMode;
        const Color headerColor = dark ? Color(150, 150, 156, 255) : Color(110, 110, 116, 255);
        const Color textColor = dark ? Color(238, 238, 242, 255) : Color(36, 36, 40, 255);

        m_header1->setColor(headerColor);
        m_header2->setColor(headerColor);
        m_zoomLabel->setColor(textColor);
        m_fullScreenLabel->setColor(textColor);
    }

protected:
    void pointerEnter() override
    {
        View::pointerEnter();
        cancelDismissTimer(m_header);
    }

    void pointerLeave() override
    {
        View::pointerLeave();
        m_hoveredAction = TileAction::None;
        requestPaint();
        startDismissTimer(m_header);
    }

    void pointerMove(const PointerEvent& event) override
    {
        View::pointerMove(event);
        cancelDismissTimer(m_header);
        const TileAction act = actionAt(event.position);
        if (m_hoveredAction != act) {
            m_hoveredAction = act;
            requestPaint();
        }
    }

    void pointerPress(const PointerEvent&) override
    {
    }

    void pointerRelease(const PointerEvent& event) override
    {
        if (event.button == MouseButton::Left) {
            const TileAction act = actionAt(event.position);
            if (act != TileAction::None) {
                executeAction(act);
                if (m_header)
                    m_header->hideTilingMenu(true);
            }
        }
    }

    void paint(Painter& painter) override
    {
        updateColors();
        const bool dark = Theme::current().darkMode;
        const RectF rect = localRect();

        // macOS Sequoia style floating frosted card
        const Color bg = dark ? Color(32, 32, 36, 242) : Color(246, 246, 248, 242);
        const Color border = dark ? Color(255, 255, 255, 34) : Color(0, 0, 0, 26);
        const Color divider = dark ? Color(255, 255, 255, 22) : Color(0, 0, 0, 18);
        const Color hoverBg = dark ? Color(255, 255, 255, 26) : Color(0, 0, 0, 16);
        const Color activeBlue = Color(0, 122, 255, 245);
        const Color monitorBorder = dark ? Color(130, 130, 136, 180) : Color(155, 155, 162, 190);
        const Color screenBg = dark ? Color(46, 46, 52, 180) : Color(228, 228, 234, 190);
        const Color iconColor = dark ? Color(230, 230, 235, 220) : Color(45, 45, 50, 220);

        // Body squircle with border
        painter.drawSmoothRectangle(rect, 14.0, bg, 1.0, border);

        // Dividers
        painter.drawHorizontalLine(14, 72, rect.width - 28, divider);
        painter.drawHorizontalLine(14, 138, rect.width - 28, divider);

        // Helper to draw miniature monitor screen with high fidelity
        const auto drawMiniScreen = [&](const RectF& slot, TileAction act, double subX, double subY, double subW, double subH) {
            const bool isHovered = (m_hoveredAction == act);
            if (isHovered)
                painter.drawSmoothRectangle(slot, 6.0, hoverBg, 0.5, border);

            // Screen container inside slot: 36 x 25
            const RectF screen(slot.x + (slot.width - 36) / 2.0, slot.y + (slot.height - 25) / 2.0, 36.0, 25.0);
            const Color activeBezel = isHovered ? Color(0, 122, 255, 180) : monitorBorder;
            painter.drawSmoothRectangle(screen, 3.5, screenBg, 1.0, activeBezel);

            // Inactive regions (desktop tile backplate)
            const RectF inner(screen.x + 1.5, screen.y + 1.5, 33.0, 22.0);
            painter.drawSmoothRectangle(inner, 2.0, dark ? Color(255, 255, 255, 14) : Color(0, 0, 0, 10));

            // Highlighted blue active tile
            const RectF fillRect(inner.x + subX * inner.width + (subX > 0 ? 0.5 : 0),
                                 inner.y + subY * inner.height + (subY > 0 ? 0.5 : 0),
                                 subW * inner.width - (subW < 1.0 ? 0.5 : 0),
                                 subH * inner.height - (subH < 1.0 ? 0.5 : 0));
            const Color blue = isHovered ? Color(10, 132, 255, 255) : activeBlue;
            painter.drawSmoothRectangle(fillRect, 2.0, blue);
        };

        // Draw 4 halves
        drawMiniScreen({14, 28, 48, 38}, TileAction::LeftHalf, 0.0, 0.0, 0.5, 1.0);
        drawMiniScreen({68, 28, 48, 38}, TileAction::RightHalf, 0.5, 0.0, 0.5, 1.0);
        drawMiniScreen({122, 28, 48, 38}, TileAction::TopHalf, 0.0, 0.0, 1.0, 0.5);
        drawMiniScreen({176, 28, 48, 38}, TileAction::BottomHalf, 0.0, 0.5, 1.0, 0.5);

        // Draw 4 quarters
        drawMiniScreen({14, 94, 48, 38}, TileAction::TopLeft, 0.0, 0.0, 0.5, 0.5);
        drawMiniScreen({68, 94, 48, 38}, TileAction::TopRight, 0.5, 0.0, 0.5, 0.5);
        drawMiniScreen({122, 94, 48, 38}, TileAction::BottomLeft, 0.0, 0.5, 0.5, 0.5);
        drawMiniScreen({176, 94, 48, 38}, TileAction::BottomRight, 0.5, 0.5, 0.5, 0.5);

        // Action rows hover washes
        const RectF row1(10, 144, rect.width - 20, 28);
        if (m_hoveredAction == TileAction::Zoom)
            painter.drawSmoothRectangle(row1, 6.0, hoverBg);

        const RectF row2(10, 176, rect.width - 20, 28);
        if (m_hoveredAction == TileAction::FullScreen)
            painter.drawSmoothRectangle(row2, 6.0, hoverBg);

        const int iconSide = std::lround(16 * painter.scale());

        // Draw Zoom/Restore icon
        const bool isMax = m_target && m_target->states().maximized;
        const std::string zoomIconName = isMax ? "window-tile-restore" : "window-tile-fill";
        sk_sp<SkImage> zoomImg = IconLoader::instance().glyph(zoomIconName, iconSide, iconColor, painter.scale());
        if (zoomImg) {
            const Rect iconDev = painter.snappedRect(RectF(16, 150, 16, 16));
            painter.drawImage(zoomImg.get(), iconDev.x, iconDev.y);
        } else {
            painter.drawRoundedRect(RectF(16, 151, 16, 12), {2.0, 2.0, Corners::Circular}, iconColor, 1.2);
            if (isMax)
                painter.drawRoundedRect(RectF(19, 154, 10, 6), {1.0, 1.0, Corners::Circular}, iconColor.withAlpha(0.65), 1.0);
            else
                painter.drawSmoothRectangle(RectF(18, 153, 12, 8), 1.0, iconColor);
        }

        // Draw Full Screen icon
        const bool isFs = m_target && m_target->states().fullScreen;
        const std::string fsIconName = isFs ? "window-tile-fullscreen-exit" : "window-tile-fullscreen";
        sk_sp<SkImage> fsImg = IconLoader::instance().glyph(fsIconName, iconSide, iconColor, painter.scale());
        if (fsImg) {
            const Rect iconDev = painter.snappedRect(RectF(16, 182, 16, 16));
            painter.drawImage(fsImg.get(), iconDev.x, iconDev.y);
        } else {
            if (isFs) {
                painter.drawLine({25, 183}, {21, 187}, iconColor, 1.3);
                painter.drawLine({21, 184}, {21, 187}, iconColor, 1.3);
                painter.drawLine({21, 187}, {24, 187}, iconColor, 1.3);
                painter.drawLine({19, 197}, {23, 193}, iconColor, 1.3);
                painter.drawLine({23, 196}, {23, 193}, iconColor, 1.3);
                painter.drawLine({23, 193}, {20, 193}, iconColor, 1.3);
            } else {
                painter.drawLine({17, 189}, {23, 183}, iconColor, 1.3);
                painter.drawLine({19, 183}, {23, 183}, iconColor, 1.3);
                painter.drawLine({23, 183}, {23, 187}, iconColor, 1.3);
                painter.drawLine({27, 191}, {21, 197}, iconColor, 1.3);
                painter.drawLine({25, 197}, {21, 197}, iconColor, 1.3);
                painter.drawLine({21, 197}, {21, 193}, iconColor, 1.3);
            }
        }
    }

private:
    TileAction actionAt(PointF pos) const
    {
        // Halves
        if (RectF(14, 28, 48, 38).contains(pos)) return TileAction::LeftHalf;
        if (RectF(68, 28, 48, 38).contains(pos)) return TileAction::RightHalf;
        if (RectF(122, 28, 48, 38).contains(pos)) return TileAction::TopHalf;
        if (RectF(176, 28, 48, 38).contains(pos)) return TileAction::BottomHalf;

        // Quarters
        if (RectF(14, 94, 48, 38).contains(pos)) return TileAction::TopLeft;
        if (RectF(68, 94, 48, 38).contains(pos)) return TileAction::TopRight;
        if (RectF(122, 94, 48, 38).contains(pos)) return TileAction::BottomLeft;
        if (RectF(176, 94, 48, 38).contains(pos)) return TileAction::BottomRight;

        // Action rows
        if (RectF(10, 144, 216, 28).contains(pos)) return TileAction::Zoom;
        if (RectF(10, 176, 216, 28).contains(pos)) return TileAction::FullScreen;

        return TileAction::None;
    }

    void executeAction(TileAction act)
    {
        if (!m_target)
            return;

        switch (act) {
        case TileAction::LeftHalf:
        case TileAction::RightHalf: {
            if (m_target->states().maximized)
                m_target->setMaximized(false);
            if (m_target->states().fullScreen)
                m_target->setFullScreen(false);
            const SizeF avail = m_target->availableSize();
            m_target->resize(SizeF(std::floor(avail.width / 2.0), avail.height));
            break;
        }
        case TileAction::TopHalf:
        case TileAction::BottomHalf: {
            if (m_target->states().maximized)
                m_target->setMaximized(false);
            if (m_target->states().fullScreen)
                m_target->setFullScreen(false);
            const SizeF avail = m_target->availableSize();
            m_target->resize(SizeF(avail.width, std::floor(avail.height / 2.0)));
            break;
        }
        case TileAction::TopLeft:
        case TileAction::TopRight:
        case TileAction::BottomLeft:
        case TileAction::BottomRight: {
            if (m_target->states().maximized)
                m_target->setMaximized(false);
            if (m_target->states().fullScreen)
                m_target->setFullScreen(false);
            const SizeF avail = m_target->availableSize();
            m_target->resize(SizeF(std::floor(avail.width / 2.0), std::floor(avail.height / 2.0)));
            break;
        }
        case TileAction::Zoom:
            if (m_target->states().fullScreen)
                m_target->setFullScreen(false);
            m_target->setMaximized(!m_target->states().maximized);
            break;
        case TileAction::FullScreen:
            m_target->setFullScreen(!m_target->states().fullScreen);
            break;
        case TileAction::None:
            break;
        }
    }

    Window* m_target;
    WindowHeader* m_header;
    TileAction m_hoveredAction = TileAction::None;
    Label* m_header1 = nullptr;
    Label* m_header2 = nullptr;
    Label* m_zoomLabel = nullptr;
    Label* m_fullScreenLabel = nullptr;
};

class TrafficLightButton : public HeaderButton
{
public:
    TrafficLightButton(const std::string& artwork, Color plateColor, Color glyphColor)
        : HeaderButton(artwork, kButton)
        , m_plateColor(plateColor)
        , m_glyphColor(glyphColor)
    {
    }

    std::function<bool()> isGroupHovered;
    std::function<void()> onHoverChanged;

    bool isHovered() const { return m_hovered || m_pressed || m_checked; }

    void pointerEnter() override
    {
        HeaderButton::pointerEnter();
        if (onHoverChanged) onHoverChanged();
    }

    void pointerLeave() override
    {
        HeaderButton::pointerLeave();
        if (onHoverChanged) onHoverChanged();
    }

    void paint(Painter& painter) override
    {
        const Window* window = this->window();
        const bool active = window && window->isActive();

        const double size = 14.0;
        const RectF logical((localRect().width - size) / 2.0, (localRect().height - size) / 2.0, size, size);

        const bool pressed = m_pressed || m_checked;
        const bool groupHovered = (isGroupHovered && isGroupHovered()) || m_hovered || pressed;
        Color color = m_plateColor;
        if (pressed) color = color.darker(1.20);

        // Crisp 1px outer border for pixel-perfect clarity
        const Color rimColor = Color(0, 0, 0, 64);

        // Draw solid vibrant circle plate with crisp border
        painter.drawSmoothRectangle(logical, size / 2.0, color, 1.0, rimColor);

        // If hovered, draw crisp vector glyphs (macOS style)
        if (groupHovered) {
            const std::string artworkName = currentArtwork();
            if (artworkName.empty()) return;
            Color glyphInk = m_glyphColor;
            if (pressed) glyphInk = glyphInk.darker(1.2);

            const double cx = logical.x + logical.width / 2.0;
            const double cy = logical.y + logical.height / 2.0;

            if (artworkName == "close") {
                // Diagonal cross
                painter.drawLine({cx - 3.2, cy - 3.2}, {cx + 3.2, cy + 3.2}, glyphInk, 1.6);
                painter.drawLine({cx + 3.2, cy - 3.2}, {cx - 3.2, cy + 3.2}, glyphInk, 1.6);
            } else if (artworkName == "minimize") {
                // Horizontal minus line
                painter.drawLine({cx - 3.6, cy}, {cx + 3.6, cy}, glyphInk, 1.6);
            } else if (artworkName == "maximize") {
                // Opposing diagonal triangles
                Path tr;
                tr.moveTo(cx - 0.5, cy - 3.2);
                tr.lineTo(cx + 3.2, cy - 3.2);
                tr.lineTo(cx + 3.2, cy + 0.5);
                tr.close();
                painter.fillPath(tr, glyphInk);

                Path bl;
                bl.moveTo(cx - 3.2, cy - 0.5);
                bl.lineTo(cx - 3.2, cy + 3.2);
                bl.lineTo(cx + 0.5, cy + 3.2);
                bl.close();
                painter.fillPath(bl, glyphInk);
            } else if (artworkName == "restore") {
                // Inward diagonal triangles
                Path tr;
                tr.moveTo(cx + 0.5, cy - 3.2);
                tr.lineTo(cx + 0.5, cy + 0.5);
                tr.lineTo(cx + 3.2, cy + 0.5);
                tr.close();
                painter.fillPath(tr, glyphInk);

                Path bl;
                bl.moveTo(cx - 0.5, cy + 3.2);
                bl.lineTo(cx - 0.5, cy - 0.5);
                bl.lineTo(cx - 3.2, cy - 0.5);
                bl.close();
                painter.fillPath(bl, glyphInk);
            }
        }
    }

protected:
    Color m_plateColor;
    Color m_glyphColor;
};

class CloseTrafficLightButton final : public TrafficLightButton
{
public:
    CloseTrafficLightButton()
        : TrafficLightButton("close", Color::rgb(0xFF5F56), Color::rgb(0x4B0000))
    {
    }
};

class MinimizeTrafficLightButton final : public TrafficLightButton
{
public:
    MinimizeTrafficLightButton()
        : TrafficLightButton("minimize", Color::rgb(0xFFBD2E), Color::rgb(0x965000))
    {
    }
};

class MaximizeTrafficLightButton final : public TrafficLightButton
{
public:
    MaximizeTrafficLightButton()
        : TrafficLightButton("maximize", Color::rgb(0x27C93F), Color::rgb(0x005F00))
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
    auto* closeBtn = static_cast<TrafficLightButton*>(m_close);
    auto* minBtn = static_cast<TrafficLightButton*>(m_minimize);
    auto* maxBtn = static_cast<TrafficLightButton*>(m_maximize);

    auto groupHovered = [closeBtn, minBtn, maxBtn] {
        return (closeBtn && closeBtn->isHovered()) ||
               (minBtn && minBtn->isHovered()) ||
               (maxBtn && maxBtn->isHovered());
    };
    auto onHover = [closeBtn, minBtn, maxBtn] {
        if (closeBtn) closeBtn->requestPaint();
        if (minBtn) minBtn->requestPaint();
        if (maxBtn) maxBtn->requestPaint();
    };

    closeBtn->isGroupHovered = groupHovered;
    closeBtn->onHoverChanged = onHover;

    minBtn->isGroupHovered = groupHovered;
    minBtn->onHoverChanged = onHover;

    auto& state = getHeaderState(this);
    state.hoverTimer->onTimeout = [this] {
        showTilingMenu();
    };
    state.dismissTimer->onTimeout = [this] {
        hideTilingMenu(false);
    };

    maxBtn->isGroupHovered = groupHovered;
    maxBtn->onHoverChanged = [this, closeBtn, minBtn, maxBtn] {
        if (closeBtn) closeBtn->requestPaint();
        if (minBtn) minBtn->requestPaint();
        if (maxBtn) maxBtn->requestPaint();

        auto& st = getHeaderState(this);
        if (maxBtn && maxBtn->isHovered()) {
            st.dismissTimer->stop();
            if (!st.popoverWindow && !st.hoverTimer->isActive())
                st.hoverTimer->start();
        } else {
            st.hoverTimer->stop();
            if (st.popoverWindow)
                st.dismissTimer->start();
        }
    };

    m_minimize->onClicked = [this] {
        hideTilingMenu(true);
        if (Window* window = this->window())
            window->minimize();
    };
    m_maximize->onClicked = [this] {
        hideTilingMenu(true);
        if (Window* window = this->window())
            window->setMaximized(!window->states().maximized);
    };
    m_close->onClicked = [this] {
        hideTilingMenu(true);
        Window* window = this->window();
        if (window && window->onCloseRequested)
            window->onCloseRequested();
        else if (window && !window->isPopup())
            Application::instance()->quit();
    };
}

WindowHeader::~WindowHeader()
{
    removeHeaderState(this);
}

void WindowHeader::hideTilingMenu(bool force)
{
    auto it = s_headerStates.find(this);
    if (it == s_headerStates.end() || !it->second->popoverWindow)
        return;

    auto& state = *(it->second);
    auto* maxBtn = static_cast<TrafficLightButton*>(m_maximize);
    if (!force) {
        if ((maxBtn && maxBtn->isHovered()) || (state.popoverView && state.popoverView->isHovered())) {
            return;
        }
    }

    state.dismissTimer->stop();
    state.hoverTimer->stop();

    std::shared_ptr<Window> win(std::move(state.popoverWindow));
    state.popoverView = nullptr;
    win->hide();
    Application::instance()->loop().post([win] {});
}

void WindowHeader::showTilingMenu()
{
    Window* win = window();
    if (!win)
        return;

    auto& state = getHeaderState(this);
    if (state.popoverWindow)
        return;

    auto* maxBtn = static_cast<TrafficLightButton*>(m_maximize);
    if (!maxBtn || !maxBtn->isHovered())
        return;

    const RectF local = m_maximize->localRect();
    const PointF topLeft = m_maximize->mapToWindow({local.x, local.y});

    PopupPlacement placement;
    placement.anchorRect = {std::round(topLeft.x), std::round(topLeft.y) - 4,
                            std::ceil(local.width), std::ceil(local.height) + 8};
    placement.anchor = PopupAnchor::BottomLeft;
    placement.gravity = PopupAnchor::BottomRight;
    placement.flipX = false;

    state.popoverWindow = Window::createPopup(win, placement, false);
    if (!state.popoverWindow)
        return;

    auto view = std::make_unique<SnapPopoverView>(win, this);
    state.popoverView = view.get();
    state.popoverWindow->setContentView(std::move(view));
    PopupGlass::prepare(*state.popoverWindow);
    state.popoverWindow->resize({236, 214});
    state.popoverWindow->setBackgroundBlur({0, 0, 236, 214}, 14.0);

    state.popoverWindow->onCloseRequested = [this] {
        hideTilingMenu(true);
    };

    state.popoverWindow->show();
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
    hideTilingMenu(true);
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
