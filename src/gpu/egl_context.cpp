// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gpu/egl_context.h"

#include <EGL/eglext.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <wayland-egl.h>

#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/egl/GrGLMakeEGLInterface.h"

namespace cutefish::ui::gpu {

// One config with alpha serves every window: popups are translucent around
// their rounded body, and opaque windows say so with their opaque region.
static EGLConfig chooseConfig(EGLDisplay display)
{
    const EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE,
    };
    EGLint count = 0;
    if (!eglChooseConfig(display, attributes, nullptr, 0, &count) || count == 0)
        return nullptr;

    std::vector<EGLConfig> configs(count);
    eglChooseConfig(display, attributes, configs.data(), count, &count);
    for (EGLConfig config : configs) {
        EGLint alpha = 0;
        EGLint red = 0;
        eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &alpha);
        eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red);
        if (alpha == 8 && red == 8)
            return config;
    }
    return configs.front();
}

std::unique_ptr<EglContext> EglContext::create(wl_display* display)
{
    std::unique_ptr<EglContext> context(new EglContext);

    context->m_display = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, display, nullptr);
    if (context->m_display == EGL_NO_DISPLAY
        || !eglInitialize(context->m_display, nullptr, nullptr)) {
        fprintf(stderr, "cutefish-toolkit: cannot initialise EGL\n");
        return nullptr;
    }
    eglBindAPI(EGL_OPENGL_ES_API);

    context->m_config = chooseConfig(context->m_display);
    if (!context->m_config) {
        fprintf(stderr, "cutefish-toolkit: no suitable EGL config\n");
        return nullptr;
    }
    eglGetConfigAttrib(context->m_display, context->m_config, EGL_STENCIL_SIZE,
                       &context->m_stencilBits);

    const char* extensions = eglQueryString(context->m_display, EGL_EXTENSIONS);
    const auto hasExtension = [extensions](const char* name) {
        return extensions && std::strstr(extensions, name);
    };
    context->m_hasBufferAge = hasExtension("EGL_EXT_buffer_age");
    if (hasExtension("EGL_KHR_swap_buffers_with_damage")) {
        context->m_swapBuffersWithDamage = reinterpret_cast<PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC>(
            eglGetProcAddress("eglSwapBuffersWithDamageKHR"));
    } else if (hasExtension("EGL_EXT_swap_buffers_with_damage")) {
        context->m_swapBuffersWithDamage = reinterpret_cast<PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC>(
            eglGetProcAddress("eglSwapBuffersWithDamageEXT"));
    }

    const EGLint contextAttributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};
    context->m_context = eglCreateContext(context->m_display, context->m_config, EGL_NO_CONTEXT,
                                          contextAttributes);
    if (context->m_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "cutefish-toolkit: cannot create a GLES 3 context\n");
        return nullptr;
    }

    // Skia needs a current context to probe the driver; no window exists yet.
    eglMakeCurrent(context->m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, context->m_context);
    context->m_skiaContext = GrDirectContexts::MakeGL(GrGLInterfaces::MakeEGL());
    if (!context->m_skiaContext) {
        fprintf(stderr, "cutefish-toolkit: Skia cannot use the GLES context\n");
        return nullptr;
    }
    return context;
}

EglContext::~EglContext()
{
    if (m_skiaContext) {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_context);
        m_skiaContext->abandonContext();
        m_skiaContext.reset();
    }
    if (m_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_context != EGL_NO_CONTEXT)
            eglDestroyContext(m_display, m_context);
        eglTerminate(m_display);
    }
}

EGLSurface EglContext::createSurface(wl_egl_window* window)
{
    EGLSurface surface = eglCreatePlatformWindowSurface(m_display, m_config, window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        fprintf(stderr, "cutefish-toolkit: cannot create an EGL window surface\n");
        return surface;
    }
    eglMakeCurrent(m_display, surface, surface, m_context);
    eglSwapInterval(m_display, 0);
    return surface;
}

void EglContext::destroySurface(EGLSurface surface)
{
    if (surface == EGL_NO_SURFACE)
        return;
    if (eglGetCurrentSurface(EGL_DRAW) == surface)
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_context);
    eglDestroySurface(m_display, surface);
}

bool EglContext::makeCurrent(EGLSurface surface)
{
    if (eglGetCurrentSurface(EGL_DRAW) == surface && eglGetCurrentContext() == m_context)
        return true;
    return eglMakeCurrent(m_display, surface, surface, m_context);
}

int EglContext::bufferAge(EGLSurface surface) const
{
    EGLint age = 0;
    if (!m_hasBufferAge || !eglQuerySurface(m_display, surface, EGL_BUFFER_AGE_EXT, &age))
        return 0;
    return age;
}

bool EglContext::swapBuffers(EGLSurface surface, const std::vector<Rect>& damage, int height)
{
    if (!m_swapBuffersWithDamage || damage.empty())
        return eglSwapBuffers(m_display, surface);
    // EGL counts rows from the bottom.
    std::vector<EGLint> rects;
    rects.reserve(damage.size() * 4);
    for (const Rect& rect : damage) {
        rects.push_back(rect.x);
        rects.push_back(height - rect.y - rect.height);
        rects.push_back(rect.width);
        rects.push_back(rect.height);
    }
    return m_swapBuffersWithDamage(m_display, surface, rects.data(), EGLint(damage.size()));
}

} // namespace cutefish::ui::gpu
