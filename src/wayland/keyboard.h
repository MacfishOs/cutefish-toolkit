// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <functional>

#include "ui/event.h"

struct wl_keyboard;
struct xkb_compose_state;
struct xkb_compose_table;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;

namespace cutefish::ui {
class EventLoop;
}

namespace cutefish::ui::wayland {

class ShellSurface;
struct KeyboardListeners;

// The seat's keyboard: the keymap and its state, compose sequences and key
// repeat, delivered to the window with keyboard focus.
class Keyboard
{
public:
    Keyboard(EventLoop& loop, wl_keyboard* keyboard);
    ~Keyboard();

    Keyboard(const Keyboard&) = delete;
    Keyboard& operator=(const Keyboard&) = delete;

    uint32_t modifiers() const { return m_modifiers; }
    std::function<void(uint32_t serial)> onKeyPressSerial;
    ShellSurface* focus() const { return m_focus; }
    void forget(ShellSurface* shell);

private:
    friend struct KeyboardListeners;

    // Returns false while a compose sequence swallows the key.
    bool translate(uint32_t keycode, bool compose, KeyEvent* event);
    void deliver(const KeyEvent& event, bool pressed);
    void startRepeat(uint32_t keycode);
    void repeatKey();
    void stopRepeat();

    EventLoop& m_loop;
    wl_keyboard* m_keyboard;
    xkb_context* m_context = nullptr;
    xkb_keymap* m_keymap = nullptr;
    xkb_state* m_state = nullptr;
    xkb_compose_table* m_composeTable = nullptr;
    xkb_compose_state* m_composeState = nullptr;
    ShellSurface* m_focus = nullptr;
    uint32_t m_modifiers = NoModifier;

    int32_t m_repeatRate = 25;
    int32_t m_repeatDelay = 600;
    uint32_t m_repeatKeycode = 0;
    int m_repeatTimer = 0;
};

} // namespace cutefish::ui::wayland
