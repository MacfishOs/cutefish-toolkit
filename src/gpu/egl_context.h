// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <memory>
#include <vector>

#include "include/core/SkRefCnt.h"

#include "ui/geometry.h"

class GrDirectContext;
struct wl_display;
struct wl_egl_window;

namespace cutefish::ui::gpu {

// One GLES context and its Skia context, shared by every window. Windows
// make their own EGL surface current before drawing.
class EglContext
{
public:
    static std::unique_ptr<EglContext> create(wl_display* display);
    ~EglContext();

    EglContext(const EglContext&) = delete;
    EglContext& operator=(const EglContext&) = delete;

    GrDirectContext* skiaContext() const { return m_skiaContext.get(); }
    EGLDisplay display() const { return m_display; }
    EGLConfig config() const { return m_config; }
    int stencilBits() const { return m_stencilBits; }

    // Swaps never wait for a frame callback: windows pace themselves.
    EGLSurface createSurface(wl_egl_window* window);
    void destroySurface(EGLSurface surface);
    bool makeCurrent(EGLSurface surface);
    // How many frames ago the current back buffer was presented, 0 when its
    // content is unknown.
    int bufferAge(EGLSurface surface) const;
    // The damage is in device pixels from the top left of a buffer this tall.
    bool swapBuffers(EGLSurface surface, const std::vector<Rect>& damage, int height);

private:
    EglContext() = default;

    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLConfig m_config = nullptr;
    EGLContext m_context = EGL_NO_CONTEXT;
    int m_stencilBits = 0;
    bool m_hasBufferAge = false;
    PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC m_swapBuffersWithDamage = nullptr;
    sk_sp<GrDirectContext> m_skiaContext;
};

} // namespace cutefish::ui::gpu
