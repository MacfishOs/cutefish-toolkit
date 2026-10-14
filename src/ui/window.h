// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ui/event.h"
#include "ui/geometry.h"
#include "ui/image.h"

class SkCanvas;
struct SkIRect;
class SkRegion;

namespace cutefish::ui {

struct FrameDamage;
class FrameClient;
class PlatformWindow;
class TestInput;
class View;

// The values follow xdg_positioner's anchors and gravities.
enum class PopupAnchor {
    Center = 0,
    Top,
    Bottom,
    Left,
    Right,
    TopLeft,
    BottomLeft,
    TopRight,
    BottomRight,
};

// Where a popup goes, relative to its parent window. The compositor moves
// it as allowed when it would not fit on the screen.
struct PopupPlacement
{
    // In the parent window's coordinates.
    RectF anchorRect;
    // The point of the rect the popup hangs from, and the way it grows from there.
    PopupAnchor anchor = PopupAnchor::BottomLeft;
    PopupAnchor gravity = PopupAnchor::BottomRight;
    PointF offset;
    bool flip = true;
    bool slide = true;
    bool resize = false;
};

// What the compositor says about a toplevel.
struct WindowStates
{
    bool activated = false;
    bool maximized = false;
    bool fullScreen = false;
    // Snapped to a side; the edges that touch others are square.
    bool tiled = false;

    bool operator==(const WindowStates&) const = default;
};

struct FrameStatistics
{
    // CLOCK_MONOTONIC.
    uint64_t firstSwapNs = 0;
    uint64_t firstPresentedNs = 0;
    uint64_t rendered = 0;
    uint64_t presented = 0;
    uint64_t discarded = 0;
};

// A top-level window drawing its own frame, or a popup of one. It renders on
// the UI thread, once per frame callback, and a configure is answered by the
// next commit with a buffer of the new size.
class Window
{
public:
    Window();
    // A popup of the parent. With a grab it takes the keyboard, and a press
    // anywhere else closes it. Its size is the one given to resize() before
    // show().
    Window(Window* parent, const PopupPlacement& placement, bool grab = true);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void setTitle(const std::string& title);
    void setMinimumSize(SizeF size);
    void setMaximumSize(SizeF size);
    // The size the window asks for when the compositor leaves it the choice.
    void resize(SizeF size);
    // Keeps the window exactly as tall as its content at its width, measured
    // at the window's scale, since text is taller at some scales.
    void setFitsContentHeight(bool fits);
    // A colour with alpha makes the window translucent. Unset follows the theme.
    void setBackgroundColor(std::optional<Color> color);
    // The rounded body the compositor blurs behind, outlines and shadows.
    void setBackgroundBlur(const RectF& rect, double radius);
    bool isPopup() const { return m_parent != nullptr; }
    Window* parentWindow() const { return m_parent; }

    void setContentView(std::unique_ptr<View> view);
    View* contentView() const { return m_contentView.get(); }

    void show();
    void startMove();
    void setMaximized(bool maximized);
    void setFullScreen(bool fullScreen);
    void minimize();
    // Kept above the parent window and opened over it; a modal one blocks it.
    void setTransientParent(Window* parent, bool modal);
    const WindowStates& states() const { return m_states; }

    double scale() const { return m_scale; }
    bool hasKeyboardFocus() const { return m_keyboardFocus; }
    // Headless windows only; on screen the compositor sets the scale.
    void setScale(double scale);
    SizeF size() const { return m_size; }
    bool isActive() const { return m_states.activated; }

    void requestLayout();
    void requestPaint();

    View* focusView() const { return m_focusView; }
    // Runs when no view takes the key. The key is lower case, as in KeyEvent.
    int addShortcut(uint32_t modifiers, uint32_t key, std::function<void()> action);
    void removeShortcut(int id);

    // Renders the content as of the given frame time (CLOCK_MONOTONIC, now
    // when 0), off screen.
    Image grab(uint64_t frameTimeNs = 0);
    // Every view with its logical and device geometry, one per line.
    std::string layoutDump() const;

    const FrameStatistics& frameStatistics() const { return m_statistics; }

    // After the compositor changed the states.
    std::function<void()> onStatesChanged;
    // The close button of a toplevel, or a popup the compositor dismissed.
    // Unset, a toplevel quits the application and a popup does nothing.
    std::function<void()> onCloseRequested;

private:
    friend class Application;
    friend class FrameClient;
    friend class PlatformWindow;
    friend class TestInput;
    friend class View;

    struct Shortcut
    {
        int id;
        uint32_t modifiers;
        uint32_t key;
        std::function<void()> action;
    };

    void handleConfigure(int width, int height, const WindowStates& states);
    void handleScaleChanged(double scale);
    void handleCloseRequested();
    void handleFrameDone();
    void handlePresented(uint64_t timestampNs, uint32_t refreshNs);
    void handleDiscarded();
    void handlePointerEnter(double x, double y);
    void handlePointerLeave();
    void handlePointerMotion(double x, double y);
    void handlePointerButton(MouseButton button, bool pressed, uint32_t modifiers);
    void handlePointerScroll(const ScrollEvent& event);
    void handleKeyboardFocus(bool focused);
    void handleKey(const KeyEvent& event, bool pressed);
    void handleHold();
    void handleTextInput(const TextInputEvent& event);
    void handleThemeChanged();
    void updateTextInput();

    void viewRemoved(View* view);
    void viewDestroyed(View* view);

    void scheduleRender();
    void postRender();
    void render();
    void prepareFrame(uint64_t frameTimeNs);
    // Everything inside the region, or everything without one.
    void paintContent(SkCanvas* canvas, const SkRegion* region);
    void updateHover();
    void updateCursor();
    void scheduleToolTip(View* view);
    void showToolTip(View* view);
    void hideToolTip();
    void setFocusView(View* view, FocusReason reason);
    void focusNext(bool backwards);
    PointerEvent pointerEvent(const View* view) const;

    void scheduleFrame();
    bool renderFrame(uint64_t frameTimeNs);
    std::vector<Rect> paintFrame(SkCanvas* canvas, int bufferAge);
    // Device pixels the view and its children cover, untransformed below the
    // view itself; `painted` asks how they were last painted, not will be.
    SkIRect subtreeRect(const View* view, PointF origin, bool painted) const;
    // As subtreeRect(), through the view's own transform when layered.
    SkIRect placedRect(const View* view, PointF origin, bool painted) const;
    SkIRect mapThroughTransform(const View* view, const SkIRect& rect, PointF position) const;
    // Adds the rect, in the coordinates around the view, to every layer and
    // the window above it.
    void addDamage(const View* view, SkIRect rect, bool painted);
    void damageNow(const View* view);
    void collectDamage(View* view, PointF origin);
    // Theme and scale changes: the window and every texture.
    void damageAll();
    void updateLayers(View* view, PointF origin, SkCanvas* target);
    void addFrameClient(FrameClient* client);
    void removeFrameClient(FrameClient* client);
    bool hasTickingClients() const;
    void interruptFrameClients(int interruption);
    uint64_t frameTime(uint64_t now);
    void fitContentHeight();

    std::unique_ptr<PlatformWindow> m_platform;
    std::unique_ptr<FrameDamage> m_frameDamage;
    Window* m_parent = nullptr;
    // Popups keep no keyboard focus of their own: the root window's keys go
    // to the innermost grabbing popup, as the compositor leaves them.
    std::vector<Window*> m_grabbingPopups;
    std::unique_ptr<View> m_contentView;
    std::vector<FrameClient*> m_frameClients;
    std::shared_ptr<Window*> m_self;

    SizeF m_size = {640, 480};
    double m_scale = 1.0;
    bool m_fitsContentHeight = false;
    SizeF m_fittedSize;
    std::optional<Color> m_backgroundColor;
    WindowStates m_states;
    bool m_configured = false;
    bool m_layoutNeeded = true;
    bool m_needsFrame = false;
    bool m_framePending = false;
    bool m_renderPosted = false;
    bool m_rendering = false;
    FrameStatistics m_statistics;
    uint64_t m_frameTimeNs = 0;
    uint64_t m_refreshNs = 16666667;

    PointF m_pointer;
    // Layout passes re-hover only a pointer still over the window.
    bool m_pointerInside = false;
    View* m_hovered = nullptr;
    View* m_pressed = nullptr;
    // A touchpad gesture stays with the view that took its first movement.
    View* m_scrollTarget = nullptr;
    MouseButton m_pressedButton = MouseButton::Left;
    uint32_t m_modifiers = NoModifier;
    uint64_t m_lastPressNs = 0;
    PointF m_lastPressPosition;
    MouseButton m_lastPressButton = MouseButton::Left;
    int m_clickCount = 0;
    // Unset after the pointer enters, so the next update sends it.
    int m_cursorShape = -1;

    // Painted over the content, inside the window, as FishUI's tool tips are.
    std::unique_ptr<View> m_toolTip;
    View* m_toolTipOwner = nullptr;
    // A press puts the tip away until the pointer moves to another view.
    View* m_toolTipDismissed = nullptr;
    int m_toolTipTimer = 0;

    View* m_focusView = nullptr;
    FocusReason m_focusReason = FocusReason::Other;
    bool m_keyboardFocus = false;
    std::vector<Shortcut> m_shortcuts;
    int m_nextShortcutId = 1;
};

} // namespace cutefish::ui
