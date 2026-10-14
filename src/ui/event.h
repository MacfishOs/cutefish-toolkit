// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "ui/geometry.h"

namespace cutefish::ui {

enum Modifier : uint32_t {
    NoModifier = 0,
    ShiftModifier = 1 << 0,
    ControlModifier = 1 << 1,
    AltModifier = 1 << 2,
    SuperModifier = 1 << 3,
};

enum class MouseButton {
    Left,
    Right,
    Middle,
};

struct PointerEvent
{
    // In the receiving view's coordinates.
    PointF position;
    MouseButton button = MouseButton::Left;
    uint32_t modifiers = NoModifier;
    // 2 for the second press of a double click, and so on.
    int clickCount = 1;
};

struct KeyEvent
{
    // An xkb keysym, see ui/keys.h. Shift+Tab arrives as Tab with Shift.
    uint32_t key = 0;
    // The UTF-8 text the key produces; empty for keys that produce none.
    std::string text;
    uint32_t modifiers = NoModifier;
    bool repeat = false;
};

struct ScrollEvent
{
    // Logical pixels, positive towards the end of the content (down, right).
    double deltaX = 0;
    double deltaY = 0;
    // Wheel clicks in 120ths of a click; zero for touchpads.
    int value120X = 0;
    int value120Y = 0;
    // Fingers on a touchpad: the deltas follow them one to one, and a stop
    // event ends the gesture, which is where kinetic scrolling takes over.
    bool precise = false;
    bool stop = false;
    uint32_t modifiers = NoModifier;
    // Milliseconds on the compositor's clock; 0 when unknown.
    uint32_t time = 0;
};

// What an input method may assume about a text field, for its suggestions.
enum class TextPurpose {
    Normal,
    Password,
    Number,
    Email,
    Url,
};

// A text view's state as the input method sees it.
struct TextInputState
{
    // The text around the cursor; the offsets are UTF-8 bytes into it.
    std::string surroundingText;
    size_t cursor = 0;
    size_t anchor = 0;
    // Where the cursor is, in window coordinates, so the candidates show by it.
    RectF cursorRectangle;
    TextPurpose purpose = TextPurpose::Normal;
};

// What an input method sends, applied in this order: the surrounding text
// is deleted, the commit inserted at the cursor, then the preedit shown.
struct TextInputEvent
{
    // UTF-8 bytes before and after the cursor.
    uint32_t deleteBefore = 0;
    uint32_t deleteAfter = 0;
    std::string commit;
    std::string preedit;
    // UTF-8 bytes into the preedit; negative hides the cursor.
    int preeditCursorBegin = -1;
    int preeditCursorEnd = -1;
};

enum class FocusReason {
    Mouse,
    // Tab or Shift+Tab: the focus is drawn.
    Keyboard,
    Other,
};

enum class CursorShape {
    Default,
    Pointer,
    Text,
    Move,
    Grab,
    Grabbing,
    NotAllowed,
    ResizeHorizontal,
    ResizeVertical,
};

} // namespace cutefish::ui
