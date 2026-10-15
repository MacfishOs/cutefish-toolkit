// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wayland/keyboard.h"

#include <chrono>
#include <clocale>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon.h>

#include "base/event_loop.h"
#include "wayland/shell_surface.h"

namespace cutefish::ui::wayland {

struct KeyboardListeners
{
    static void keymap(void* data, wl_keyboard*, uint32_t format, int32_t fd, uint32_t size)
    {
        auto* self = static_cast<Keyboard*>(data);
        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
            close(fd);
            return;
        }
        void* map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (map == MAP_FAILED)
            return;
        xkb_keymap* keymap = xkb_keymap_new_from_string(self->m_context,
                                                        static_cast<const char*>(map),
                                                        XKB_KEYMAP_FORMAT_TEXT_V1,
                                                        XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(map, size);
        if (!keymap)
            return;

        xkb_state_unref(self->m_state);
        xkb_keymap_unref(self->m_keymap);
        self->m_keymap = keymap;
        self->m_state = xkb_state_new(keymap);
    }

    static void enter(void* data, wl_keyboard*, uint32_t, wl_surface* surface, wl_array*)
    {
        auto* self = static_cast<Keyboard*>(data);
        self->m_focus = surface ? static_cast<ShellSurface*>(wl_surface_get_user_data(surface))
                                : nullptr;
        if (self->m_focus)
            self->m_focus->delegate()->keyboardEnter();
    }

    static void leave(void* data, wl_keyboard*, uint32_t, wl_surface*)
    {
        auto* self = static_cast<Keyboard*>(data);
        self->stopRepeat();
        if (self->m_composeState)
            xkb_compose_state_reset(self->m_composeState);
        if (self->m_focus)
            self->m_focus->delegate()->keyboardLeave();
        self->m_focus = nullptr;
    }

    static void key(void* data, wl_keyboard*, uint32_t serial, uint32_t, uint32_t key,
                    uint32_t state)
    {
        auto* self = static_cast<Keyboard*>(data);
        if (state == WL_KEYBOARD_KEY_STATE_PRESSED && self->onKeyPressSerial)
            self->onKeyPressSerial(serial);
        if (!self->m_state)
            return;
        // Evdev codes are xkb's less 8.
        const uint32_t keycode = key + 8;
        const bool pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED;

        KeyEvent event;
        if (!pressed) {
            if (keycode == self->m_repeatKeycode)
                self->stopRepeat();
            self->translate(keycode, false, &event);
            self->deliver(event, false);
            return;
        }

        if (!self->translate(keycode, true, &event))
            return;
        self->deliver(event, true);
        if (xkb_keymap_key_repeats(self->m_keymap, keycode))
            self->startRepeat(keycode);
    }

    static void modifiers(void* data, wl_keyboard*, uint32_t, uint32_t depressed, uint32_t latched,
                          uint32_t locked, uint32_t group)
    {
        auto* self = static_cast<Keyboard*>(data);
        if (!self->m_state)
            return;
        xkb_state_update_mask(self->m_state, depressed, latched, locked, 0, 0, group);

        const auto active = [self](const char* name) {
            return xkb_state_mod_name_is_active(self->m_state, name, XKB_STATE_MODS_EFFECTIVE) > 0;
        };
        uint32_t modifiers = NoModifier;
        if (active(XKB_MOD_NAME_SHIFT))
            modifiers |= ShiftModifier;
        if (active(XKB_MOD_NAME_CTRL))
            modifiers |= ControlModifier;
        if (active(XKB_MOD_NAME_ALT))
            modifiers |= AltModifier;
        if (active(XKB_MOD_NAME_LOGO))
            modifiers |= SuperModifier;
        self->m_modifiers = modifiers;
    }

    static void repeatInfo(void* data, wl_keyboard*, int32_t rate, int32_t delay)
    {
        auto* self = static_cast<Keyboard*>(data);
        self->m_repeatRate = rate;
        self->m_repeatDelay = delay;
    }

    static constexpr wl_keyboard_listener listener = {
        keymap, enter, leave, key, modifiers, repeatInfo,
    };
};

Keyboard::Keyboard(base::EventLoop& loop, wl_keyboard* keyboard)
    : m_loop(loop)
    , m_keyboard(keyboard)
    , m_context(xkb_context_new(XKB_CONTEXT_NO_FLAGS))
{
    const char* locale = std::setlocale(LC_CTYPE, nullptr);
    m_composeTable = xkb_compose_table_new_from_locale(m_context, locale ? locale : "C",
                                                       XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (m_composeTable)
        m_composeState = xkb_compose_state_new(m_composeTable, XKB_COMPOSE_STATE_NO_FLAGS);
    wl_keyboard_add_listener(keyboard, &KeyboardListeners::listener, this);
}

Keyboard::~Keyboard()
{
    stopRepeat();
    wl_keyboard_release(m_keyboard);
    xkb_compose_state_unref(m_composeState);
    xkb_compose_table_unref(m_composeTable);
    xkb_state_unref(m_state);
    xkb_keymap_unref(m_keymap);
    xkb_context_unref(m_context);
}

void Keyboard::forget(ShellSurface* shell)
{
    if (m_focus == shell) {
        stopRepeat();
        m_focus = nullptr;
    }
}

bool Keyboard::translate(uint32_t keycode, bool compose, KeyEvent* event)
{
    xkb_keysym_t symbol = xkb_state_key_get_one_sym(m_state, keycode);
    char text[64] = {};
    bool composed = false;

    if (compose && m_composeState) {
        xkb_compose_state_feed(m_composeState, symbol);
        switch (xkb_compose_state_get_status(m_composeState)) {
        case XKB_COMPOSE_COMPOSING:
            return false;
        case XKB_COMPOSE_CANCELLED:
            xkb_compose_state_reset(m_composeState);
            return false;
        case XKB_COMPOSE_COMPOSED:
            symbol = xkb_compose_state_get_one_sym(m_composeState);
            xkb_compose_state_get_utf8(m_composeState, text, sizeof(text));
            xkb_compose_state_reset(m_composeState);
            composed = true;
            break;
        case XKB_COMPOSE_NOTHING:
            break;
        }
    }
    if (!composed)
        xkb_state_key_get_utf8(m_state, keycode, text, sizeof(text));

    // Control characters are what Ctrl turns letters into, not text.
    const unsigned char first = static_cast<unsigned char>(text[0]);
    if (text[1] == 0 && (first < 0x20 || first == 0x7f))
        text[0] = 0;

    event->key = symbol == XKB_KEY_ISO_Left_Tab ? XKB_KEY_Tab : xkb_keysym_to_lower(symbol);
    event->keycode = keycode - 8;
    event->text = text;
    event->modifiers = m_modifiers;
    event->repeat = false;
    return true;
}

void Keyboard::deliver(const KeyEvent& event, bool pressed)
{
    if (m_focus)
        m_focus->delegate()->key(event, pressed);
}

void Keyboard::startRepeat(uint32_t keycode)
{
    stopRepeat();
    if (m_repeatRate <= 0)
        return;
    m_repeatKeycode = keycode;
    m_repeatTimer = m_loop.startTimer(std::chrono::milliseconds(m_repeatDelay), false, [this] {
        repeatKey();
        const auto interval = std::chrono::nanoseconds(1000000000 / m_repeatRate);
        m_repeatTimer = m_loop.startTimer(interval, true, [this] { repeatKey(); });
    });
}

void Keyboard::repeatKey()
{
    KeyEvent event;
    translate(m_repeatKeycode, false, &event);
    event.repeat = true;
    deliver(event, true);
}

void Keyboard::stopRepeat()
{
    if (m_repeatTimer)
        m_loop.stopTimer(m_repeatTimer);
    m_repeatTimer = 0;
    m_repeatKeycode = 0;
}

} // namespace cutefish::ui::wayland
