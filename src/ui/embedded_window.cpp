// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/embedded_window.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <xkbcommon/xkbcommon.h>

#include "include/core/SkColorSpace.h"
#include "include/core/SkRegion.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLTypes.h"

#include "ui/application.h"
#include "ui/embedded_gpu_private.h"
#include "ui/embedded_platform_window.h"
#include "ui/layer.h"
#include "ui/text_context.h"
#include "ui/view.h"

namespace cutefish::ui {

static uint64_t monotonicNs()
{
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return uint64_t(now.tv_sec) * 1000000000 + uint64_t(now.tv_nsec);
}

void EmbeddedPlatformWindow::show(int width, int height)
{
    notifyConfigure(width, height, m_states);
}

void EmbeddedPlatformWindow::setCursorShape(CursorShape shape)
{
    if (onCursorShape)
        onCursorShape(shape);
}

void EmbeddedPlatformWindow::requestRender()
{
    if (m_renderRequested)
        return;
    m_renderRequested = true;
    if (onRenderRequested)
        onRenderRequested();
}

bool EmbeddedPlatformWindow::present(SizeF size, double scale, bool opaque, const PaintFunction& paint)
{
    const int width = int(std::lround(size.width * scale));
    const int height = int(std::lround(size.height * scale));
    if (width <= 0 || height <= 0)
        return false;
    // Glyphs over a translucent frame stay greyscale.
    const SkSurfaceProps properties = Application::instance()->text().surfaceProps(opaque);

    if (m_gpuTarget) {
        if (m_gpuTarget->width != width || m_gpuTarget->height != height)
            return false;
        GrGLTextureInfo info;
        info.fTarget = 0x0DE1; // GL_TEXTURE_2D
        info.fID = m_gpuTarget->texture;
        info.fFormat = 0x8058; // GL_RGBA8
        const GrBackendTexture texture = GrBackendTextures::MakeGL(width, height, skgpu::Mipmapped::kNo, info);
        // Top-left origin: rows lie in the texture as an upload of the CPU frame would.
        const sk_sp<SkSurface> surface = SkSurfaces::WrapBackendTexture(
            m_gpuTarget->context, texture, kTopLeft_GrSurfaceOrigin, 0, kRGBA_8888_SkColorType, nullptr,
            &properties);
        if (!surface)
            return false;
        const bool holdsFrame = m_gpuTarget->keepsLastFrame && m_lastTexture == m_gpuTarget->texture;
        paint(surface->getCanvas(), holdsFrame ? 1 : 0);
        m_gpuTarget->context->flushAndSubmit(surface.get(), GrSyncCpu::kNo);
        m_lastTexture = m_gpuTarget->texture;
        m_holdsFrame = false;
        return true;
    }

    if (width != m_width || height != m_height) {
        m_width = width;
        m_height = height;
        m_pixels.assign(size_t(width) * size_t(height) * 4, 0);
        m_holdsFrame = false;
    }
    const SkImageInfo info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    const sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(info, m_pixels.data(), size_t(width) * 4,
                                                            &properties);
    if (!surface)
        return false;
    paint(surface->getCanvas(), m_holdsFrame ? 1 : 0);
    m_holdsFrame = true;
    m_lastTexture = 0;
    return true;
}

EmbeddedWindow::EmbeddedWindow()
{
    EmbeddedPlatformWindow* embedded = platform();
    if (!embedded) {
        fprintf(stderr, "EmbeddedWindow needs Application::Platform::Embedded\n");
        std::abort();
    }
    // What the host composites it over shows through.
    setBackgroundColor(Color{0, 0, 0, 0});
    embedded->onRenderRequested = [this] {
        if (onRenderRequested)
            onRenderRequested();
    };
    embedded->onCursorShape = [this](CursorShape shape) {
        if (shape == m_reportedShape)
            return;
        m_reportedShape = shape;
        if (onCursorShapeChanged)
            onCursorShapeChanged(shape);
    };
}

EmbeddedWindow::~EmbeddedWindow()
{
    if (EmbeddedPlatformWindow* embedded = platform()) {
        embedded->onRenderRequested = nullptr;
        embedded->onCursorShape = nullptr;
    }
}

EmbeddedPlatformWindow* EmbeddedWindow::platform() const
{
    return m_platform && m_platform->isEmbedded() ? static_cast<EmbeddedPlatformWindow*>(m_platform.get())
                                                  : nullptr;
}

void EmbeddedWindow::setSize(SizeF size)
{
    m_shown = true;
    m_configured = true;
    if (size == m_size)
        return;
    m_size = size;
    m_layoutNeeded = true;
    m_frameDamage->full = true;
    scheduleFrame();
}

void EmbeddedWindow::setKeyboardFocus(bool focused)
{
    if (focused != m_keyboardFocus)
        handleKeyboardFocus(focused);
}

void EmbeddedWindow::setActive(bool active)
{
    if (active == m_states.activated)
        return;
    m_states.activated = active;
    Application::instance()->scheduleActiveUpdate();
    if (onStatesChanged)
        onStatesChanged();
}

void EmbeddedWindow::updateLayout()
{
    if (layoutIfNeeded() && m_pointerInside)
        updateHover();
}

bool EmbeddedWindow::needsRender() const
{
    return m_configured && m_contentView && (m_needsFrame || m_layoutNeeded || hasTickingClients());
}

const EmbeddedWindow::Frame& EmbeddedWindow::render(uint64_t frameTimeNs)
{
    EmbeddedPlatformWindow* embedded = platform();
    embedded->clearRenderRequest();
    m_frame.damage.clear();
    if (!m_configured || !m_contentView || !renderFrame(frameTimeNs ? frameTimeNs : monotonicNs()))
        return m_frame;
    m_frame.pixels = embedded->pixels();
    m_frame.width = embedded->width();
    m_frame.height = embedded->height();
    m_frame.stride = size_t(embedded->width()) * 4;
    finishFrame();
    return m_frame;
}

const EmbeddedWindow::Frame* EmbeddedWindow::render(EmbeddedGpu& gpu, const GpuTexture& texture,
                                                    uint64_t frameTimeNs)
{
    if (!m_configured || !m_contentView || texture.width != pixelWidth() || texture.height != pixelHeight())
        return nullptr;
    EmbeddedGpuScope scope(gpu);
    if (!scope.isValid())
        return nullptr;
    EmbeddedPlatformWindow* embedded = platform();
    const EmbeddedPlatformWindow::GpuTarget target{scope.context(), texture.id, texture.width, texture.height,
                                                   texture.keepsLastFrame};
    embedded->setGpuTarget(&target);
    embedded->clearRenderRequest();
    const bool rendered = renderFrame(frameTimeNs ? frameTimeNs : monotonicNs());
    embedded->setGpuTarget(nullptr);
    if (!rendered)
        return nullptr;
    m_frame.pixels = nullptr;
    m_frame.width = texture.width;
    m_frame.height = texture.height;
    m_frame.stride = 0;
    m_frame.damage.clear();
    finishFrame();
    return &m_frame;
}

void EmbeddedWindow::finishFrame()
{
    if (!m_frameDamage->history.empty()) {
        for (SkRegion::Iterator it(m_frameDamage->history.front()); !it.done(); it.next()) {
            const SkIRect& rect = it.rect();
            m_frame.damage.push_back({rect.x(), rect.y(), rect.width(), rect.height()});
        }
    }
    // Running animations want the next frame as well.
    if (m_needsFrame || hasTickingClients())
        platform()->requestRender();
}

int EmbeddedWindow::pixelWidth() const
{
    return int(std::lround(m_size.width * m_scale));
}

int EmbeddedWindow::pixelHeight() const
{
    return int(std::lround(m_size.height * m_scale));
}

void EmbeddedWindow::pointerEnter(PointF position)
{
    handlePointerEnter(position.x, position.y);
}

void EmbeddedWindow::pointerMove(PointF position)
{
    if (!m_pointerInside)
        handlePointerEnter(position.x, position.y);
    else
        handlePointerMotion(position.x, position.y);
}

void EmbeddedWindow::pointerLeave()
{
    if (!m_pointerInside)
        return;
    handlePointerLeave();
    // Nothing in the window asks for a shape any more.
    if (m_reportedShape != CursorShape::Default) {
        m_reportedShape = CursorShape::Default;
        if (onCursorShapeChanged)
            onCursorShapeChanged(m_reportedShape);
    }
}

void EmbeddedWindow::pointerButton(MouseButton button, bool pressed, uint32_t modifiers)
{
    handlePointerButton(button, pressed, modifiers);
}

void EmbeddedWindow::scroll(const ScrollEvent& event)
{
    handlePointerScroll(event);
}

void EmbeddedWindow::key(const KeyEvent& event, bool pressed)
{
    KeyEvent normalized = event;
    normalized.key = event.key == XKB_KEY_ISO_Left_Tab ? XKB_KEY_Tab : xkb_keysym_to_lower(event.key);
    // Control characters are what Ctrl turns letters into, not text.
    if (normalized.text.size() == 1) {
        const auto first = static_cast<unsigned char>(normalized.text[0]);
        if (first < 0x20 || first == 0x7f)
            normalized.text.clear();
    }
    handleKey(normalized, pressed);
}

} // namespace cutefish::ui
