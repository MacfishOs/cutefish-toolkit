// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

namespace cutefish::ui {

// Skia's GPU backend on a GLES context the host owns and keeps current, for
// EmbeddedWindow::render() into the host's textures: no context of the
// toolkit's own and no sharing. Windows and this object free GL objects when
// destroyed, so destroy them with the context current, or after abandon().
//
// The GL state contract, per render into a texture:
// - Before drawing, Skia forgets all the GL state it cached
//   (GrDirectContext::resetContext), as the host changed it since.
// - After drawing, the framebuffer bindings, the viewport, the scissor box
//   and test, the clear colour and dithering are what they were before.
// - Everything else is left as follows, which the host may rely on:
//   program 0; vertex array 0 with every attribute disabled, divisor 0 and
//   pointer (4, FLOAT, unnormalised, stride 0, no buffer, offset 0); array,
//   element, pixel pack and unpack buffers 0; texture unit 0 active, 2D and
//   external textures and samplers unbound on every unit up to 16; blending
//   off with (ONE, ONE_MINUS_SRC_ALPHA) and ADD; colour mask all; depth,
//   stencil, cull face, polygon offset, alpha-to-coverage and primitive
//   restart off; pixel store alignments 4 and row lengths 0.
// - Drawing is submitted (glFlush) but not waited for; the host's later
//   commands on the same context see the texture finished.
class EmbeddedGpu
{
public:
    // Over the context current on the calling thread; null when there is
    // none or Skia cannot use it.
    static std::unique_ptr<EmbeddedGpu> createForCurrentContext();
    ~EmbeddedGpu();

    EmbeddedGpu(const EmbeddedGpu&) = delete;
    EmbeddedGpu& operator=(const EmbeddedGpu&) = delete;

    // The context was lost or is about to be destroyed without being made
    // current again: nothing touches GL from here on. Renders into textures
    // fail; render on the CPU, or create another EmbeddedGpu for a new
    // context.
    void abandon();
    // With the context current: frees what Skia keeps on the GPU (glyph
    // atlases, uploaded images, layer textures it made), then abandons, as
    // before the host destroys the context, e.g. leaving the VT.
    void release();
    // Abandoned, by abandon() or release(), or after Skia saw a reset of a
    // robust context.
    bool isAbandoned() const;
    // With the context current: drops what Skia caches but does not need
    // now, keeping the context usable.
    void purgeResources();

private:
    friend class EmbeddedGpuScope;
    struct Private;

    EmbeddedGpu();

    std::unique_ptr<Private> d;
};

} // namespace cutefish::ui
