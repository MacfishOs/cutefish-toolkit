// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <EGL/egl.h>

#include "include/core/SkRefCnt.h"

#include "ui/embedded_gpu.h"

class GrDirectContext;

namespace cutefish::ui {

struct EmbeddedGpu::Private
{
    sk_sp<GrDirectContext> context;
    // The host's context; drawing waits until it is current again.
    EGLContext egl = EGL_NO_CONTEXT;
    bool es3 = false;
    bool externalTextures = false;
    // What the state reset walks over.
    int units = 8;
    int attributes = 8;
    bool abandoned = false;
};

// The host's GL state around one render into its texture, as the contract in
// embedded_gpu.h states it.
class EmbeddedGpuScope
{
public:
    // Fails, drawing nothing, unless the gpu's context is current and usable.
    explicit EmbeddedGpuScope(EmbeddedGpu& gpu);
    ~EmbeddedGpuScope();

    EmbeddedGpuScope(const EmbeddedGpuScope&) = delete;
    EmbeddedGpuScope& operator=(const EmbeddedGpuScope&) = delete;

    bool isValid() const { return m_valid; }
    GrDirectContext* context() const;

private:
    EmbeddedGpu& m_gpu;
    bool m_valid = false;
    int m_framebuffer = 0;
    int m_readFramebuffer = 0;
    int m_viewport[4] = {};
    int m_scissor[4] = {};
    bool m_scissorTest = false;
    bool m_dither = true;
    float m_clearColor[4] = {};
};

} // namespace cutefish::ui
