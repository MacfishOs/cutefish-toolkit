// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "ui/platform_window.h"

class GrDirectContext;

namespace cutefish::ui {

// The platform side of a window on Application::Platform::Embedded: frames go
// into memory the host reads, and the host hears when one is wanted and which
// cursor the pointer asks for.
class EmbeddedPlatformWindow final : public PlatformWindow
{
public:
    using PlatformWindow::PlatformWindow;

    bool isHeadless() const override { return false; }
    bool isEmbedded() const override { return true; }
    void setTitle(const std::string&) override {}
    void setAppId(const std::string&) override {}
    void setMinimumSize(int, int) override {}
    void setMaximumSize(int, int) override {}
    void setMaximized(bool) override {}
    void setFullScreen(bool) override {}
    void minimize() override {}
    void setTransientParent(PlatformWindow*, bool) override {}
    void startMove() override {}
    void setBlur(const RectF&, double) override {}
    // No input method: the host's keys carry the text.
    void setTextInput(const TextInputState*) override {}
    void show(int width, int height) override;
    void setCursorShape(CursorShape shape) override;
    void requestRender() override;
    bool present(SizeF size, double scale, bool opaque, const PaintFunction& paint) override;

    const uint8_t* pixels() const { return m_pixels.data(); }
    int width() const { return m_width; }
    int height() const { return m_height; }
    // The host takes the next request as a new one once it rendered.
    void clearRenderRequest() { m_renderRequested = false; }
    bool isRenderRequested() const { return m_renderRequested; }

    // Set around a render into a host texture on the GPU.
    struct GpuTarget
    {
        GrDirectContext* context = nullptr;
        uint32_t texture = 0;
        int width = 0;
        int height = 0;
        bool keepsLastFrame = false;
    };
    void setGpuTarget(const GpuTarget* target) { m_gpuTarget = target; }

    std::function<void()> onRenderRequested;
    std::function<void(CursorShape)> onCursorShape;

private:
    WindowStates m_states;
    std::vector<uint8_t> m_pixels;
    int m_width = 0;
    int m_height = 0;
    // The buffer holds the last frame, so frames repaint only what changed.
    bool m_holdsFrame = false;
    bool m_renderRequested = false;
    const GpuTarget* m_gpuTarget = nullptr;
    // The texture the last frame went into, 0 after a frame in memory.
    uint32_t m_lastTexture = 0;
};

} // namespace cutefish::ui
