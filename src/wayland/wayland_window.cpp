// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wayland/wayland_window.h"

#include <GLES3/gl3.h>
#include <cmath>
#include <wayland-egl.h>

#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLTypes.h"

#include "gpu/egl_context.h"
#include "ui/application.h"
#include "ui/text_context.h"
#include "wayland/connection.h"
#include "wayland/text_input.h"

namespace cutefish::ui::wayland {

WaylandWindow::WaylandWindow(Window* window)
    : PlatformWindow(window)
    , m_shell(std::make_unique<ShellSurface>(Application::instance()->connection(),
                                            static_cast<ShellSurface::Delegate*>(this)))
{
}

WaylandWindow::~WaylandWindow()
{
    auto& gpu = Application::instance()->gpu();
    if (m_eglSurface != EGL_NO_SURFACE) {
        gpu.makeCurrent(m_eglSurface);
        m_surface.reset();
        gpu.destroySurface(m_eglSurface);
    }
    if (m_eglWindow)
        wl_egl_window_destroy(m_eglWindow);
}

void WaylandWindow::setTitle(const std::string& title)
{
    if (m_shell)
        m_shell->setTitle(title);
}

void WaylandWindow::setAppId(const std::string& appId)
{
    if (m_shell)
        m_shell->setAppId(appId);
}

void WaylandWindow::setMinimumSize(int width, int height)
{
    if (m_shell)
        m_shell->setMinimumSize(width, height);
}

WaylandWindow::WaylandWindow(Window* window, WaylandWindow* parent,
                             const PopupPlacement& placement, bool grab)
    : PlatformWindow(window)
    , m_parent(parent)
    , m_placement(placement)
    , m_grab(grab)
{
}

void WaylandWindow::setMaximumSize(int width, int height)
{
    if (m_shell)
        m_shell->setMaximumSize(width, height);
}

void WaylandWindow::setMaximized(bool maximized)
{
    if (m_shell)
        m_shell->setMaximized(maximized);
}

void WaylandWindow::setFullScreen(bool fullScreen)
{
    if (m_shell)
        m_shell->setFullScreen(fullScreen);
}

void WaylandWindow::minimize()
{
    if (m_shell)
        m_shell->minimize();
}

void WaylandWindow::setTransientParent(PlatformWindow* parent, bool modal)
{
    auto* waylandParent = static_cast<WaylandWindow*>(parent);
    if (m_shell)
        m_shell->setParent(waylandParent ? waylandParent->m_shell.get() : nullptr, modal);
}

void WaylandWindow::show(int width, int height)
{
    if (!m_shell && m_parent) {
        m_shell = std::make_unique<ShellSurface>(Application::instance()->connection(),
                                                 static_cast<ShellSurface::Delegate*>(this),
                                                 m_parent->m_shell.get(), m_placement, width,
                                                 height, m_grab);
        m_shell->setBlur(m_blurRect, m_blurRadius);
    }
    m_shell->map();
}

void WaylandWindow::setBlur(const RectF& rect, double radius)
{
    m_blurRect = rect;
    m_blurRadius = radius;
    if (m_shell)
        m_shell->setBlur(rect, radius);
}

void WaylandWindow::startMove()
{
    if (m_shell)
        m_shell->startMove();
}

void WaylandWindow::setCursorShape(CursorShape shape)
{
    if (m_shell)
        m_shell->connection().setCursorShape(m_shell.get(), shape);
}

void WaylandWindow::setTextInput(const TextInputState* state)
{
    if (!m_shell)
        return;
    if (TextInput* input = m_shell->connection().textInput())
        input->update(m_shell.get(), state);
}

bool WaylandWindow::present(SizeF size, double scale, bool opaque, const PaintFunction& paint)
{
    if (!m_shell)
        return false;
    auto* application = Application::instance();
    auto& gpu = application->gpu();
    const int width = int(std::lround(size.width * scale));
    const int height = int(std::lround(size.height * scale));
    if (!m_eglWindow) {
        m_eglWindow = wl_egl_window_create(m_shell->surface(), width, height);
        m_eglSurface = gpu.createSurface(m_eglWindow);
    } else if (width != m_bufferWidth || height != m_bufferHeight) {
        // Takes effect with the next buffer, which is the one drawn below.
        wl_egl_window_resize(m_eglWindow, width, height, 0, 0);
        m_surface.reset();
    }
    m_bufferWidth = width;
    m_bufferHeight = height;

    if (m_eglSurface == EGL_NO_SURFACE || !gpu.makeCurrent(m_eglSurface))
        return false;

    if (!m_surface) {
        GrGLFramebufferInfo framebuffer;
        framebuffer.fFBOID = 0;
        framebuffer.fFormat = GL_RGBA8;
        const GrBackendRenderTarget target =
            GrBackendRenderTargets::MakeGL(width, height, 0, gpu.stencilBits(), framebuffer);
        const SkSurfaceProps properties(0, application->text().pixelGeometry());
        m_surface = SkSurfaces::WrapBackendRenderTarget(gpu.skiaContext(), target,
                                                        kBottomLeft_GrSurfaceOrigin,
                                                        kRGBA_8888_SkColorType, nullptr,
                                                        &properties);
        if (!m_surface)
            return false;
    }

    // Asked before drawing: the query fixes which buffer is the back one.
    const int bufferAge = gpu.bufferAge(m_eglSurface);
    const std::vector<Rect> damage = paint(m_surface->getCanvas(), bufferAge);
    gpu.skiaContext()->flushAndSubmit(m_surface.get());

    m_shell->prepareCommit(int(size.width), int(size.height), opaque);
    gpu.swapBuffers(m_eglSurface, damage, height);
    return true;
}

void WaylandWindow::configure(int width, int height, const WindowStates& states)
{
    notifyConfigure(width, height, states);
}

void WaylandWindow::scaleChanged(double scale)
{
    notifyScaleChanged(scale);
}

void WaylandWindow::closeRequested()
{
    notifyCloseRequested();
}

void WaylandWindow::frameDone()
{
    notifyFrameDone();
}

void WaylandWindow::presented(uint64_t timestampNs, uint32_t refreshNs)
{
    notifyPresented(timestampNs, refreshNs);
}

void WaylandWindow::discarded()
{
    notifyDiscarded();
}

void WaylandWindow::pointerEnter(double x, double y)
{
    notifyPointerEnter(x, y);
}

void WaylandWindow::pointerLeave()
{
    notifyPointerLeave();
}

void WaylandWindow::pointerMotion(double x, double y)
{
    notifyPointerMotion(x, y);
}

void WaylandWindow::pointerButton(MouseButton button, bool pressed, uint32_t modifiers)
{
    notifyPointerButton(button, pressed, modifiers);
}

void WaylandWindow::pointerScroll(const ScrollEvent& event)
{
    notifyPointerScroll(event);
}

void WaylandWindow::keyboardEnter()
{
    notifyKeyboardEnter();
}

void WaylandWindow::keyboardLeave()
{
    notifyKeyboardLeave();
}

void WaylandWindow::key(const KeyEvent& event, bool pressed)
{
    notifyKey(event, pressed);
}

void WaylandWindow::pointerHold()
{
    notifyHold();
}

void WaylandWindow::textInput(const TextInputEvent& event)
{
    notifyTextInput(event);
}

} // namespace cutefish::ui::wayland
