// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <EGL/egl.h>
#include <memory>

#include "include/core/SkRefCnt.h"

#include "ui/platform_window.h"
#include "wayland/shell_surface.h"

class SkSurface;
struct wl_egl_window;

namespace cutefish::ui::wayland {

// An xdg_toplevel drawn with GLES through Skia.
class WaylandWindow final : public PlatformWindow, private ShellSurface::Delegate
{
public:
    explicit WaylandWindow(Window* window);
    WaylandWindow(Window* window, WaylandWindow* parent, const PopupPlacement& placement, bool grab);
    ~WaylandWindow() override;

    bool isHeadless() const override { return false; }
    void setTitle(const std::string& title) override;
    void setAppId(const std::string& appId) override;
    void setMinimumSize(int width, int height) override;
    void setMaximumSize(int width, int height) override;
    void setMaximized(bool maximized) override;
    void setFullScreen(bool fullScreen) override;
    void minimize() override;
    void activate() override;
    void exportHandle(std::function<void(const std::string&)> done) override;
    SizeF screenSize() const override;
    void setTransientParent(PlatformWindow* parent, bool modal) override;
    void setRestoreSize(int width, int height) override;
    void show(int width, int height) override;
    void startMove() override;
    void setCursorShape(CursorShape shape) override;
    void setBlur(const RectF& rect, double radius) override;
    void setTextInput(const TextInputState* state) override;

    bool present(SizeF size, double scale, bool opaque, const PaintFunction& paint) override;

private:
    void configure(int width, int height, const WindowStates& states) override;
    void scaleChanged(double scale) override;
    void closeRequested() override;
    void frameDone() override;
    void presented(uint64_t timestampNs, uint32_t refreshNs) override;
    void discarded() override;
    void pointerEnter(double x, double y) override;
    void pointerLeave() override;
    void pointerMotion(double x, double y) override;
    void pointerButton(MouseButton button, bool pressed, uint32_t modifiers) override;
    void pointerScroll(const ScrollEvent& event) override;
    void keyboardEnter() override;
    void keyboardLeave() override;
    void key(const KeyEvent& event, bool pressed) override;
    void pointerHold() override;
    void textInput(const TextInputEvent& event) override;
    void configureBounds(int width, int height) override;
    void interactiveResize(bool active) override;

    std::unique_ptr<ShellSurface> m_shell;
    // A popup is made on show(), when its size is known.
    WaylandWindow* m_parent = nullptr;
    PopupPlacement m_placement;
    bool m_grab = false;
    RectF m_blurRect;
    double m_blurRadius = 0;
    wl_egl_window* m_eglWindow = nullptr;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    sk_sp<SkSurface> m_surface;
    int m_bufferWidth = 0;
    int m_bufferHeight = 0;
};

} // namespace cutefish::ui::wayland
