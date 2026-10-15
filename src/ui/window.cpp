// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/window.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cxxabi.h>
#include <typeinfo>

#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include "base/event_loop.h"
#include "ui/application.h"
#include "ui/frame_client.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/layer.h"
#include "ui/painter.h"
#include "ui/platform_window.h"
#include "ui/text_context.h"
#include "ui/theme.h"
#include "ui/view.h"

namespace cutefish::ui {

static constexpr uint64_t kDoubleClickNs = 400000000;
static constexpr auto kToolTipDelay = std::chrono::milliseconds(700);
// FishUI's tool tip: six pixels of padding, a small spacing and a half above
// what it explains.
static constexpr double kToolTipPadding = 6;
static constexpr double kToolTipGap = 9;
static constexpr double kToolTipMargin = 6;
static constexpr double kToolTipShadowSigma = 2.75;

namespace {

class ToolTipView final : public View
{
public:
    explicit ToolTipView(std::string text)
        : m_label(addChild<Label>(std::move(text)))
    {
    }

    SizeF sizeHint() const override
    {
        const SizeF text = m_label->sizeHint();
        return {text.width + 2 * kToolTipPadding, text.height + 2 * kToolTipPadding};
    }

    RectF visualRect() const override
    {
        const double reach = Painter::shadowReach(kToolTipShadowSigma);
        return localRect().adjusted(-reach, -reach, reach, reach);
    }

protected:
    void layout() override
    {
        m_label->setGeometry(localRect().adjusted(kToolTipPadding, kToolTipPadding,
                                                  -kToolTipPadding, -kToolTipPadding));
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        painter.drawShadow(localRect(), theme.smallRadius, kToolTipShadowSigma, Color::rgb(0).withAlpha(0.15));
        painter.fillRoundedRect(localRect(), theme.smallRadius, theme.secondBackground.withAlpha(0.95),
                                themeCorners());
    }

private:
    Label* m_label;
};

} // namespace
static constexpr double kDoubleClickDistance = 4;

static uint64_t monotonicNs()
{
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return uint64_t(now.tv_sec) * 1000000000 + uint64_t(now.tv_nsec);
}

static std::string typeName(const View& view)
{
    const char* mangled = typeid(view).name();
    int status = 0;
    char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    std::string name = status == 0 ? demangled : mangled;
    free(demangled);
    const std::string prefix = "cutefish::ui::";
    if (name.starts_with(prefix))
        name.erase(0, prefix.size());
    return name;
}

Window::Window()
    : m_frameDamage(std::make_unique<FrameDamage>())
    , m_self(std::make_shared<Window*>(this))
{
    m_platform = PlatformWindow::create(this, nullptr, {}, false);
    m_platform->setAppId(Application::instance()->applicationId());
    Application::instance()->m_windows.push_back(this);
}

static bool familyActivated(const Window* window);

Window::Window(Window* parent, const PopupPlacement& placement, bool grab)
    : m_frameDamage(std::make_unique<FrameDamage>())
    , m_parent(parent)
    , m_self(std::make_shared<Window*>(this))
    // Text measured before the popup hears its own scale fits at the parent's.
    , m_scale(parent->m_scale)
{
    m_platform = PlatformWindow::create(this, parent->m_platform.get(), placement, grab);
    if (grab)
        parent->m_grabbingPopups.push_back(this);
    // The keyboard goes where the root's goes.
    m_keyboardFocus = parent->m_keyboardFocus;
    Application::instance()->m_windows.push_back(this);
    m_active = familyActivated(this);
}

Window::~Window()
{
    // A window closed right after a resize still keeps it.
    if (m_saveTimer) {
        Application::instance()->loop().stopTimer(m_saveTimer);
        m_saveTimer = 0;
        saveSize();
    }
    std::erase(Application::instance()->m_windows, this);
    for (Window* window : Application::instance()->m_windows) {
        if (window->m_transientParent == this)
            window->m_transientParent = nullptr;
    }
    hideToolTip();
    if (m_parent)
        std::erase(m_parent->m_grabbingPopups, this);
    for (Window* popup : m_grabbingPopups)
        popup->m_parent = nullptr;
    m_self.reset();
    for (FrameClient* client : m_frameClients)
        client->m_window = nullptr;
    // Views tell the window when they go, so it must outlive them.
    m_overlay.reset();
    m_contentView.reset();
    m_platform.reset();
    // The family may have lost its focused member; asked after the surface
    // went, so the compositor's next focus is in by the answer.
    if (m_states.activated)
        Application::instance()->scheduleActiveUpdate();
}

void Window::setTitle(const std::string& title)
{
    m_platform->setTitle(title);
}

void Window::setMinimumSize(SizeF size)
{
    m_minimumSize = size;
    m_platform->setMinimumSize(int(std::ceil(size.width)), int(std::ceil(size.height)));
}

void Window::setMaximumSize(SizeF size)
{
    m_maximumSize = size;
    m_platform->setMaximumSize(int(std::floor(size.width)), int(std::floor(size.height)));
}

void Window::setMaximized(bool maximized)
{
    m_platform->setMaximized(maximized);
}

void Window::setFullScreen(bool fullScreen)
{
    m_platform->setFullScreen(fullScreen);
}

void Window::minimize()
{
    m_platform->minimize();
}

// The window its transient parents and popup parents lead to.
static const Window* familyRoot(const Window* window)
{
    for (;;) {
        const Window* parent = window->transientParent() ? window->transientParent() : window->parentWindow();
        if (!parent)
            return window;
        window = parent;
    }
}

// From the compositor's activation states as they stand; isActive() takes it
// only once a batch of configures is in.
static bool familyActivated(const Window* window)
{
    const Window* root = familyRoot(window);
    for (const Window* member : Application::instance()->windows()) {
        if (member->states().activated && familyRoot(member) == root)
            return true;
    }
    return false;
}

bool Window::isActive() const
{
    return m_active;
}

void Window::updateActive()
{
    const bool active = familyActivated(this);
    if (active == m_active)
        return;
    m_active = active;
    m_frameDamage->full = true;
    requestPaint();
    if (onActiveChanged)
        onActiveChanged();
}

void Window::setTransientParent(Window* parent, bool modal)
{
    m_transientParent = parent;
    m_platform->setTransientParent(parent ? parent->m_platform.get() : nullptr, modal);
    m_active = familyActivated(this);
    // Opened over the parent, it most likely shows at the parent's scale.
    if (parent && !m_configured)
        handleScaleChanged(parent->m_scale);
}

void Window::resize(SizeF size)
{
    m_size = {std::round(size.width), std::round(size.height)};
    requestLayout();
}

void Window::setFitsContentHeight(bool fits)
{
    m_fitsContentHeight = fits;
    m_fittedSize = {};
    requestLayout();
}

void Window::fitContentHeight()
{
    if (!m_fitsContentHeight || !m_contentView)
        return;
    // Sums of device-pixel heights at a fractional scale land a hair above
    // whole numbers, e.g. 3 × 25 / 1.5.
    m_size.height = std::ceil(m_contentView->heightForWidth(m_size.width) - 1e-6);
    if (m_size == m_fittedSize)
        return;
    m_fittedSize = m_size;
    setMinimumSize(m_size);
    setMaximumSize(m_size);
}

void Window::setBackgroundColor(std::optional<Color> color)
{
    m_backgroundColor = color;
    requestPaint();
}

void Window::handleThemeChanged()
{
    // Font sizes and colours may have changed.
    requestLayout();
    damageAll();
}

void Window::setBackgroundBlur(const RectF& rect, double radius)
{
    m_platform->setBlur(rect, radius);
    requestPaint();
}

// Qt Quick's Overlay: every child covers the window and places its own views.
class OverlayView final : public View
{
protected:
    void layout() override
    {
        for (const auto& child : children())
            child->setGeometry(localRect());
    }
};

View* Window::overlay()
{
    if (!m_overlay) {
        m_overlay = std::make_unique<OverlayView>();
        m_overlay->m_window = this;
        requestLayout();
    }
    return m_overlay.get();
}

View* Window::viewAt(PointF point, bool input) const
{
    // Only views that take the pointer stop it; elsewhere it reaches the content.
    if (m_overlay) {
        if (View* view = m_overlay->viewAt(point, true))
            return input ? view : m_overlay->viewAt(point, false);
    }
    return m_contentView ? m_contentView->viewAt(point, input) : nullptr;
}

void Window::setContentView(std::unique_ptr<View> view)
{
    m_hovered = nullptr;
    for (View* hovered : m_hoverChain)
        hovered->m_hoverTracked = false;
    m_hoverChain.clear();
    m_pressed = nullptr;
    m_contentView = std::move(view);
    if (m_contentView)
        m_contentView->m_window = this;
    requestLayout();
    requestPaint();
}

void Window::activate()
{
    m_platform->activate();
}

void Window::exportHandle(std::function<void(const std::string& handle)> done)
{
    m_platform->exportHandle(std::move(done));
}

void Window::show()
{
    // As QWindow::show on a shown window: nothing, the size stays.
    if (m_shown)
        return;
    m_shown = true;
    restoreSize();
    fitContentHeight();
    m_platform->show(int(std::ceil(m_size.width)), int(std::ceil(m_size.height)));
}

void Window::startMove()
{
    // The compositor takes the pointer for the move and sends no release.
    m_pressed = nullptr;
    m_platform->startMove();
}

void Window::setScale(double scale)
{
    if (m_platform->isHeadless() || m_platform->isEmbedded())
        handleScaleChanged(scale);
}

SizeF Window::screenSize() const
{
    if (!m_platform->isHeadless() && !m_platform->isEmbedded())
        return m_platform->screenSize();
    if (m_screenSize.width <= 0 && m_parent)
        return m_parent->screenSize();
    return m_screenSize;
}

void Window::requestLayout()
{
    m_layoutNeeded = true;
    scheduleFrame();
}

void Window::requestPaint()
{
    m_frameDamage->full = true;
    scheduleFrame();
}

void Window::scheduleFrame()
{
    m_needsFrame = true;
    scheduleRender();
}

Image Window::grab(uint64_t frameTimeNs)
{
    if (!m_contentView)
        return {};
    prepareFrame(frameTimeNs ? frameTimeNs : monotonicNs());

    Image image(int(std::lround(m_size.width * m_scale)), int(std::lround(m_size.height * m_scale)));
    // The background is opaque, so premultiplied pixels are the unpremultiplied ones.
    const SkImageInfo info = SkImageInfo::Make(image.width(), image.height(),
                                               kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    const SkSurfaceProps properties = Application::instance()->text().surfaceProps();
    const sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
        info, image.bits(), size_t(image.width()) * 4, &properties);
    if (!surface)
        return {};
    paintContent(surface->getCanvas(), nullptr);
    return image;
}

std::string Window::layoutDump() const
{
    std::string dump;
    const auto dumpView = [&](const auto& self, const View& view, PointF origin, int depth) -> void {
        const RectF& geometry = view.geometry();
        const PointF position = {origin.x + geometry.x, origin.y + geometry.y};
        const auto device = [this](double logical) { return long(std::lround(logical * m_scale)); };
        const long left = device(position.x);
        const long top = device(position.y);
        char line[256];
        snprintf(line, sizeof(line), "%*s%s%s%s%s logical %g %g %g %g device %ld %ld %ld %ld%s\n",
                 depth * 2, "", typeName(view).c_str(), view.name().empty() ? "" : " \"",
                 view.name().c_str(), view.name().empty() ? "" : "\"", geometry.x, geometry.y,
                 geometry.width, geometry.height, left, top,
                 device(position.x + geometry.width) - left,
                 device(position.y + geometry.height) - top, view.isVisible() ? "" : " hidden");
        dump += line;
        for (const auto& child : view.children())
            self(self, *child, position, depth + 1);
    };
    if (m_contentView)
        dumpView(dumpView, *m_contentView, {}, 0);
    return dump;
}

void Window::handleConfigure(int width, int height, const WindowStates& states)
{
    if (width > 0 && height > 0 && (width != m_size.width || height != m_size.height)) {
        m_size = {double(width), double(height)};
        m_layoutNeeded = true;
        m_frameDamage->full = true;
        if (m_interactiveResize)
            m_resized = true;
    } else if ((width <= 0 || height <= 0) && !m_boundsApplied && !m_restoreId.empty() && m_bounds.width > 0
               && m_bounds.height > 0) {
        // The first configure leaves the size to the window, which keeps a
        // restored size inside the work area.
        m_boundsApplied = true;
        m_size = {std::min(m_size.width, m_bounds.width), std::min(m_size.height, m_bounds.height)};
        m_layoutNeeded = true;
    }
    const bool maximizedChanged = states.maximized != m_states.maximized || states.fullScreen != m_states.fullScreen;
    const bool changed = !(states == m_states);
    const bool activationChanged = states.activated != m_states.activated;
    WindowStates others = states;
    others.activated = m_states.activated;
    if (!(others == m_states))
        m_frameDamage->full = true;
    m_states = states;
    // Focus rings and title colours follow isActive(), once the whole batch
    // of configures is in: focus passing to a dialog leaves the family active.
    if (activationChanged)
        Application::instance()->scheduleActiveUpdate();
    m_configured = true;
    if (maximizedChanged)
        saveMaximized();
    if (changed && onStatesChanged)
        onStatesChanged();
    m_needsFrame = true;
    // Answered without waiting for a frame callback: the compositor waits
    // for the commit that acks it.
    if (m_platform->isEmbedded())
        scheduleRender();
    else if (!m_platform->isHeadless())
        postRender();
}

void Window::handleScaleChanged(double scale)
{
    if (scale == m_scale)
        return;
    m_scale = scale;
    // Text is shaped at the device size.
    requestLayout();
    damageAll();
}

void Window::handleCloseRequested()
{
    if (onCloseRequested)
        onCloseRequested();
    else if (!isPopup())
        Application::instance()->quit();
}

void Window::handleFrameDone()
{
    m_framePending = false;
    if (m_needsFrame || hasTickingClients())
        postRender();
}

void Window::handlePresented(uint64_t timestampNs, uint32_t refreshNs)
{
    if (refreshNs > 0)
        m_refreshNs = refreshNs;
    ++m_statistics.presented;
    if (!m_statistics.firstPresentedNs)
        m_statistics.firstPresentedNs = timestampNs;
}

void Window::handleDiscarded()
{
    ++m_statistics.discarded;
}

void Window::handlePointerEnter(double x, double y)
{
    m_pointer = {x, y};
    m_pointerInside = true;
    m_cursorShape = -1;
    updateHover();
}

void Window::handlePointerLeave()
{
    m_pointerInside = false;
    hideToolTip();
    m_pressed = nullptr;
    updateHoverChain();
    if (m_hovered) {
        View* hovered = m_hovered;
        m_hovered = nullptr;
        hovered->pointerLeave();
    }
}

void Window::handlePointerMotion(double x, double y)
{
    m_pointer = {x, y};
    m_pointerInside = true;
    if (m_pressed)
        m_pressed->pointerMove(pointerEvent(m_pressed));
    updateHover();
}

void Window::handlePointerButton(MouseButton button, bool pressed, uint32_t modifiers)
{
    m_modifiers = modifiers;
    if (!m_contentView)
        return;

    if (!pressed) {
        if (m_pressed && button == m_pressedButton) {
            View* view = m_pressed;
            m_pressed = nullptr;
            PointerEvent event = pointerEvent(view);
            event.button = button;
            view->pointerRelease(event);
        }
        updateHover();
        return;
    }

    interruptFrameClients(int(FrameClient::Interruption::Input));
    m_toolTipDismissed = m_toolTipOwner;
    hideToolTip();
    if (onPointerPressed)
        onPointerPressed(m_pointer, button);

    // Presses close together in time and place count up, for double clicks.
    const uint64_t now = monotonicNs();
    const bool repeated = m_clickCount > 0 && button == m_lastPressButton
            && now - m_lastPressNs < kDoubleClickNs
            && std::hypot(m_pointer.x - m_lastPressPosition.x,
                          m_pointer.y - m_lastPressPosition.y) <= kDoubleClickDistance;
    m_clickCount = repeated ? m_clickCount + 1 : 1;
    m_lastPressNs = now;
    m_lastPressPosition = m_pointer;
    m_lastPressButton = button;

    // A second button while one is held goes to the view holding the first.
    View* target = m_pressed ? m_pressed : viewAt(m_pointer, true);
    if (!m_pressed) {
        m_pressed = target;
        m_pressedButton = button;
    }
    for (View* view = target; view; view = view->parent()) {
        if (view->acceptsFocus() && view->isEnabled()) {
            setFocusView(view, FocusReason::Mouse);
            break;
        }
    }
    if (target) {
        PointerEvent event = pointerEvent(target);
        event.button = button;
        target->pointerPress(event);
    }
    updateCursor();
}

void Window::handlePointerScroll(const ScrollEvent& event)
{
    m_modifiers = event.modifiers;
    hideToolTip();
    if (!m_contentView)
        return;

    // The rest of a touchpad gesture goes where its start went, wherever
    // the pointer is now.
    if (event.precise && m_scrollTarget) {
        View* target = m_scrollTarget;
        if (event.stop)
            m_scrollTarget = nullptr;
        target->scroll(event);
        return;
    }
    // A new contact stops every coast in the window.
    if (event.precise && !event.stop)
        interruptFrameClients(int(FrameClient::Interruption::ScrollBegan));
    for (View* view = viewAt(m_pointer, false); view; view = view->parent()) {
        if (view->isEnabled() && view->scroll(event)) {
            if (event.precise && !event.stop)
                m_scrollTarget = view;
            return;
        }
    }
}

void Window::handleKeyboardFocus(bool focused)
{
    for (Window* popup : m_grabbingPopups)
        popup->handleKeyboardFocus(focused);
    m_keyboardFocus = focused;
    if (!focused)
        interruptFrameClients(int(FrameClient::Interruption::Deactivated));
    updateTextInput();
    // The caret follows the keyboard; the rest of the window follows isActive().
    if (m_focusView)
        m_focusView->requestPaint();
}

void Window::handleTextInput(const TextInputEvent& event)
{
    if (m_focusView && m_focusView->isEnabled() && m_focusView->acceptsTextInput())
        m_focusView->textInput(event);
}

void Window::updateTextInput()
{
    View* view = m_focusView;
    if (m_keyboardFocus && view && view->isEnabled() && view->acceptsTextInput()) {
        const TextInputState state = view->textInputState();
        m_platform->setTextInput(&state);
    } else {
        m_platform->setTextInput(nullptr);
    }
}

void Window::handleHold()
{
    interruptFrameClients(int(FrameClient::Interruption::FingersDown));
}

void Window::handleKey(const KeyEvent& event, bool pressed)
{
    if (!m_grabbingPopups.empty()) {
        m_grabbingPopups.back()->handleKey(event, pressed);
        return;
    }
    m_modifiers = event.modifiers;
    if (!pressed) {
        for (View* view = m_focusView; view; view = view->parent()) {
            if (view->keyRelease(event))
                return;
        }
        return;
    }

    interruptFrameClients(int(FrameClient::Interruption::Input));
    hideToolTip();
    for (View* view = m_focusView; view; view = view->parent()) {
        if (view->isEnabled() && view->keyPress(event))
            return;
    }

    if (event.key == key::Tab && !(event.modifiers & ~ShiftModifier)) {
        focusNext(event.modifiers & ShiftModifier);
        return;
    }

    // An action may change the list.
    const std::vector<Shortcut> shortcuts = m_shortcuts;
    for (const Shortcut& shortcut : shortcuts) {
        if (shortcut.key == event.key && shortcut.modifiers == event.modifiers) {
            shortcut.action();
            return;
        }
    }
}

int Window::addShortcut(uint32_t modifiers, uint32_t key, std::function<void()> action)
{
    const int id = m_nextShortcutId++;
    m_shortcuts.push_back({id, modifiers, key, std::move(action)});
    return id;
}

void Window::removeShortcut(int id)
{
    std::erase_if(m_shortcuts, [id](const Shortcut& shortcut) { return shortcut.id == id; });
}

void Window::setFocusView(View* view, FocusReason reason)
{
    if (view == m_focusView) {
        if (reason != m_focusReason) {
            m_focusReason = reason;
            requestPaint();
        }
        return;
    }
    View* previous = m_focusView;
    m_focusView = view;
    m_focusReason = reason;
    if (previous)
        previous->focusOut();
    if (view)
        view->focusIn(reason);
    updateTextInput();
    requestPaint();
}

void Window::focusNext(bool backwards)
{
    if (!m_contentView)
        return;
    std::vector<View*> chain;
    const auto collect = [&chain](const auto& self, View* view) -> void {
        if (!view->isVisible() || !view->m_enabled)
            return;
        if (view->acceptsFocus() && !view->m_clickFocusOnly)
            chain.push_back(view);
        for (const auto& child : view->children())
            self(self, child.get());
    };
    collect(collect, m_contentView.get());
    if (m_overlay)
        collect(collect, m_overlay.get());
    if (chain.empty())
        return;

    const auto it = std::find(chain.begin(), chain.end(), m_focusView);
    size_t index = 0;
    if (it == chain.end())
        index = backwards ? chain.size() - 1 : 0;
    else if (backwards)
        index = (size_t(it - chain.begin()) + chain.size() - 1) % chain.size();
    else
        index = (size_t(it - chain.begin()) + 1) % chain.size();
    setFocusView(chain[index], FocusReason::Keyboard);
}

PointerEvent Window::pointerEvent(const View* view) const
{
    PointerEvent event;
    event.position = view->mapFromWindow(m_pointer);
    event.button = m_pressedButton;
    event.modifiers = m_modifiers;
    event.clickCount = std::max(1, m_clickCount);
    return event;
}

void Window::viewRemoved(View* view)
{
    if (m_toolTipOwner && view->contains(m_toolTipOwner))
        hideToolTip();
    if (m_toolTipDismissed && view->contains(m_toolTipDismissed))
        m_toolTipDismissed = nullptr;
    if (m_scrollTarget && view->contains(m_scrollTarget))
        m_scrollTarget = nullptr;
    if (m_hovered && view->contains(m_hovered))
        m_hovered = nullptr;
    std::erase_if(m_hoverChain, [view](View* hovered) {
        if (!view->contains(hovered))
            return false;
        hovered->m_hoverTracked = false;
        return true;
    });
    if (m_pressed && view->contains(m_pressed))
        m_pressed = nullptr;
    if (m_focusView && view->contains(m_focusView))
        setFocusView(nullptr, FocusReason::Other);
}

void Window::viewDestroyed(View* view)
{
    if (m_toolTipDismissed == view)
        m_toolTipDismissed = nullptr;
    if (m_toolTipOwner == view) {
        m_toolTipOwner = nullptr;
        m_toolTipPinned = false;
        if (m_toolTip) {
            damageNow(m_toolTip.get());
            m_toolTip.reset();
        }
        if (m_toolTipTimer) {
            Application::instance()->loop().stopTimer(m_toolTipTimer);
            m_toolTipTimer = 0;
        }
    }
    if (m_scrollTarget == view)
        m_scrollTarget = nullptr;
    if (m_hovered == view)
        m_hovered = nullptr;
    std::erase(m_hoverChain, view);
    if (m_pressed == view)
        m_pressed = nullptr;
    if (m_focusView == view)
        m_focusView = nullptr;
}

void Window::updateHoverChain()
{
    std::vector<View*> chain;
    View* deepest = m_contentView && m_pointerInside ? viewAt(m_pointer, false) : nullptr;
    for (View* view = deepest; view; view = view->parent()) {
        if (view->m_tracksHover && view->isEnabled() && view->localRect().contains(view->mapFromWindow(m_pointer)))
            chain.push_back(view);
    }
    const std::vector<View*> previous = std::exchange(m_hoverChain, chain);
    for (View* view : previous) {
        if (std::find(chain.begin(), chain.end(), view) == chain.end()) {
            view->m_hoverTracked = false;
            view->hoverChanged();
        }
    }
    for (View* view : chain) {
        if (!view->m_hoverTracked) {
            view->m_hoverTracked = true;
            view->hoverChanged();
        }
    }
    // Entering, a move, or the view moving under a still pointer.
    for (View* view : std::vector<View*>(m_hoverChain)) {
        if (std::find(m_hoverChain.begin(), m_hoverChain.end(), view) == m_hoverChain.end())
            continue;
        const PointerEvent event = pointerEvent(view);
        const bool entered = std::find(previous.begin(), previous.end(), view) == previous.end();
        if (!entered && view->m_hoverPosition == event.position)
            continue;
        view->m_hoverPosition = event.position;
        view->hoverMove(event);
    }
}

void Window::updateHover()
{
    updateHoverChain();
    View* view = m_contentView && m_pointerInside ? viewAt(m_pointer, true) : nullptr;
    if (view != m_hovered) {
        View* previous = m_hovered;
        m_hovered = view;
        if (previous)
            previous->pointerLeave();
        if (view)
            view->pointerEnter();
    }
    updateCursor();
    if (m_toolTipPinned)
        return;

    View* owner = m_contentView && m_pointerInside && !m_pressed ? viewAt(m_pointer, false)
                                                                 : nullptr;
    while (owner && owner->toolTip().empty())
        owner = owner->parent();
    if (owner != m_toolTipDismissed)
        m_toolTipDismissed = nullptr;
    if (owner != m_toolTipOwner && owner != m_toolTipDismissed)
        scheduleToolTip(owner);
}

void Window::scheduleToolTip(View* view)
{
    hideToolTip();
    m_toolTipOwner = view;
    if (!view)
        return;
    m_toolTipTimer = Application::instance()->loop().startTimer(
        kToolTipDelay, false, [self = std::weak_ptr<Window*>(m_self)] {
            if (const auto window = self.lock()) {
                (*window)->m_toolTipTimer = 0;
                if (View* owner = (*window)->m_toolTipOwner)
                    (*window)->showToolTip(owner, owner->localRect(), owner->toolTip());
            }
        });
}

void Window::showToolTip(View* view, const RectF& rect, const std::string& text)
{
    auto tip = std::make_unique<ToolTipView>(text);
    tip->m_window = this;
    const SizeF size = tip->sizeHint();
    const PointF topLeft = view->mapToWindow({rect.x, rect.y});
    double x = topLeft.x + (rect.width - size.width) / 2;
    double y = topLeft.y - size.height - kToolTipGap;
    x = std::clamp(x, kToolTipMargin, std::max(kToolTipMargin, m_size.width - size.width - kToolTipMargin));
    // Below when there is no room above.
    if (y < kToolTipMargin)
        y = topLeft.y + rect.height + kToolTipGap;
    tip->setGeometry({std::round(x), std::round(y), std::ceil(size.width), std::ceil(size.height)});
    tip->layoutTree();
    if (m_toolTip)
        damageNow(m_toolTip.get());
    m_toolTip = std::move(tip);
    m_toolTipOwner = view;
    m_toolTip->invalidate();
}

void Window::pinToolTip(View* view, const RectF& rect, std::string text)
{
    if (m_toolTipTimer) {
        Application::instance()->loop().stopTimer(m_toolTipTimer);
        m_toolTipTimer = 0;
    }
    m_toolTipPinned = true;
    showToolTip(view, rect, text);
    scheduleFrame();
}

void Window::unpinToolTip(View* view)
{
    if (m_toolTipPinned && m_toolTipOwner == view) {
        hideToolTip();
        m_toolTipDismissed = view;
    }
}

void Window::hideToolTip()
{
    if (m_toolTipTimer) {
        Application::instance()->loop().stopTimer(m_toolTipTimer);
        m_toolTipTimer = 0;
    }
    m_toolTipOwner = nullptr;
    m_toolTipPinned = false;
    if (m_toolTip) {
        damageNow(m_toolTip.get());
        m_toolTip.reset();
        scheduleFrame();
    }
}

void Window::updateCursor()
{
    // The view holding the pointer keeps its cursor until the release.
    CursorShape shape = CursorShape::Default;
    View* view = m_pressed ? m_pressed
                           : (m_contentView ? viewAt(m_pointer, false) : nullptr);
    for (; view; view = view->parent()) {
        if (view->isEnabled() && view->cursorShape() != CursorShape::Default) {
            shape = view->cursorShape();
            break;
        }
    }
    if (int(shape) == m_cursorShape)
        return;
    m_cursorShape = int(shape);
    m_platform->setCursorShape(shape);
}

void Window::scheduleRender()
{
    // A paint asked for while rendering is drawn on the next frame.
    if (!m_configured || m_framePending || m_rendering || m_platform->isHeadless())
        return;
    // The host renders when it composites next.
    if (m_platform->isEmbedded()) {
        m_platform->requestRender();
        return;
    }
    postRender();
}

void Window::postRender()
{
    if (m_renderPosted)
        return;
    m_renderPosted = true;
    Application::instance()->loop().post([self = std::weak_ptr<Window*>(m_self)] {
        if (const auto window = self.lock())
            (*window)->render();
    });
}

void Window::render()
{
    m_renderPosted = false;
    if (!m_configured || !m_contentView)
        return;
    renderFrame(frameTime(monotonicNs()));
}

bool Window::renderFrame(uint64_t frameTimeNs)
{
    m_rendering = true;
    prepareFrame(frameTimeNs);
    // Whatever asked for a paint until here is drawn by this frame.
    m_needsFrame = false;
    const Color background = m_backgroundColor.value_or(Theme::current().background);
    const bool presented = m_platform->present(
        m_size, m_scale, background.a == 255,
        [this](SkCanvas* canvas, int bufferAge) { return paintFrame(canvas, bufferAge); });
    m_rendering = false;
    if (!presented)
        return false;

    // Headless and embedded windows hear of no frame callbacks.
    m_framePending = !m_platform->isHeadless() && !m_platform->isEmbedded();
    ++m_statistics.rendered;
    if (!m_statistics.firstSwapNs)
        m_statistics.firstSwapNs = monotonicNs();
    return true;
}

// Frames step by whole refreshes and drift slowly towards the clock, since
// rendering starts early or late around a steady refresh.
uint64_t Window::frameTime(uint64_t now)
{
    const double interval = double(m_refreshNs);
    const double since = double(int64_t(now - m_frameTimeNs));
    if (!m_frameTimeNs || since > 3 * interval || -since > 2 * interval) {
        m_frameTimeNs = now;
    } else {
        const double predicted = double(m_frameTimeNs)
                + interval * std::max(1.0, std::round(since / interval));
        m_frameTimeNs = uint64_t(predicted + 0.1 * (double(now) - predicted));
    }
    return m_frameTimeNs;
}

void Window::prepareFrame(uint64_t frameTimeNs)
{
    // A client may stop, or go away, from its own tick.
    const std::vector<FrameClient*> clients = m_frameClients;
    for (FrameClient* client : clients) {
        if (std::find(m_frameClients.begin(), m_frameClients.end(), client) != m_frameClients.end()
            && client->m_ticking)
            client->tick(frameTimeNs);
    }

    if (layoutIfNeeded()) {
        updateHover();
    } else if (m_pointerInside) {
        // A scroll or an animation may have moved a view under the pointer.
        updateHoverChain();
    }
    if (m_contentView)
        collectDamage(m_contentView.get(), {});
    if (m_overlay)
        collectDamage(m_overlay.get(), {});
    if (m_toolTip)
        collectDamage(m_toolTip.get(), {});
}

bool Window::layoutIfNeeded()
{
    if (!m_layoutNeeded || !m_contentView)
        return false;
    fitContentHeight();
    m_contentView->setGeometry({0, 0, m_size.width, m_size.height});
    m_contentView->layoutTree();
    if (m_overlay) {
        m_overlay->setGeometry({0, 0, m_size.width, m_size.height});
        m_overlay->layoutTree();
    }
    // Geometry set by the pass itself asks for layout again.
    m_layoutNeeded = false;
    return true;
}

void Window::addFrameClient(FrameClient* client)
{
    m_frameClients.push_back(client);
}

void Window::removeFrameClient(FrameClient* client)
{
    std::erase(m_frameClients, client);
}

bool Window::hasTickingClients() const
{
    return std::any_of(m_frameClients.begin(), m_frameClients.end(),
                       [](const FrameClient* client) { return client->m_ticking; });
}

void Window::interruptFrameClients(int interruption)
{
    const std::vector<FrameClient*> clients = m_frameClients;
    for (FrameClient* client : clients) {
        if (std::find(m_frameClients.begin(), m_frameClients.end(), client) != m_frameClients.end())
            client->interrupt(FrameClient::Interruption(interruption));
    }
}

} // namespace cutefish::ui
