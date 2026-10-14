// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>

#include "ui/event.h"
#include "ui/geometry.h"
#include "ui/image.h"

namespace cutefish::ui {

class View;
class Window;

// Drives a window as the window system would, so tests can work views
// without a compositor. Positions are in window coordinates.
class TestInput
{
public:
    static void movePointer(Window& window, PointF position);
    static void leavePointer(Window& window);
    static void press(Window& window, PointF position, MouseButton button = MouseButton::Left,
                      uint32_t modifiers = NoModifier);
    static void release(Window& window, PointF position, MouseButton button = MouseButton::Left);
    static void click(Window& window, PointF position, MouseButton button = MouseButton::Left);
    static void scroll(Window& window, PointF position, const ScrollEvent& event);
    // Fingers coming to rest on the touchpad.
    static void hold(Window& window);

    static void setKeyboardFocus(Window& window, bool focused);
    static void pressKey(Window& window, const KeyEvent& event);
    static void releaseKey(Window& window, const KeyEvent& event);
    // A press and a release.
    static void typeKey(Window& window, uint32_t key, uint32_t modifiers = NoModifier,
                        const std::string& text = {});
    // Each character as a key that produces it.
    static void typeText(Window& window, const std::string& text);
    // As an input method sends it to the focused view.
    static void inputMethod(Window& window, const TextInputEvent& event);

    // Renders a frame as on screen into a chain of three buffers, repainting
    // only what changed since each buffer's last frame; returns the buffer.
    static Image presentFrame(Window& window, uint64_t frameTimeNs = 0);
    // The device pixels the view's own paint inks at the scale, without its
    // children, relative to its top left.
    static Rect inkRect(View& view, double scale);
};

} // namespace cutefish::ui
