// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wayland/connection.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <linux/input-event-codes.h>
#include <sys/epoll.h>
#include <wayland-client.h>

#include <cursor-shape-v1-client-protocol.h>
#include <cutefish-surface-blur-v1-client-protocol.h>
#include <cutefish-window-hints-v1-client-protocol.h>
#include <fractional-scale-v1-client-protocol.h>
#include <pointer-gestures-unstable-v1-client-protocol.h>
#include <text-input-unstable-v3-client-protocol.h>
#include <presentation-time-client-protocol.h>
#include <viewporter-client-protocol.h>
#include <xdg-dialog-v1-client-protocol.h>
#include <xdg-shell-client-protocol.h>

#include "base/event_loop.h"
#include "wayland/data_device.h"
#include "wayland/keyboard.h"
#include "wayland/text_input.h"
#include "wayland/shell_surface.h"

namespace cutefish::ui::wayland {

static ShellSurface* shellSurfaceFor(wl_surface* surface)
{
    // Null when the surface was destroyed while the event was in flight.
    return surface ? static_cast<ShellSurface*>(wl_surface_get_user_data(surface)) : nullptr;
}

template<typename T>
static T* bind(wl_registry* registry, uint32_t name, const wl_interface* interface,
               uint32_t offered, uint32_t wanted)
{
    return static_cast<T*>(wl_registry_bind(registry, name, interface, std::min(offered, wanted)));
}

struct ConnectionListeners
{
    static void global(void* data, wl_registry* registry, uint32_t name, const char* interface,
                       uint32_t version)
    {
        auto* self = static_cast<Connection*>(data);
        if (strcmp(interface, wl_compositor_interface.name) == 0) {
            self->m_compositor = bind<wl_compositor>(registry, name, &wl_compositor_interface,
                                                     version, 4);
        } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
            self->m_wmBase = bind<xdg_wm_base>(registry, name, &xdg_wm_base_interface, version, 5);
            xdg_wm_base_add_listener(self->m_wmBase, &wmBase, self);
        } else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
            self->m_viewporter = bind<wp_viewporter>(registry, name, &wp_viewporter_interface,
                                                     version, 1);
        } else if (strcmp(interface, wp_fractional_scale_manager_v1_interface.name) == 0) {
            self->m_fractionalScaleManager = bind<wp_fractional_scale_manager_v1>(
                registry, name, &wp_fractional_scale_manager_v1_interface, version, 1);
        } else if (strcmp(interface, wp_presentation_interface.name) == 0) {
            self->m_presentation = bind<wp_presentation>(registry, name, &wp_presentation_interface,
                                                         version, 1);
        } else if (strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0) {
            self->m_cursorShapeManager = bind<wp_cursor_shape_manager_v1>(
                registry, name, &wp_cursor_shape_manager_v1_interface, version, 1);
        } else if (strcmp(interface, zwp_pointer_gestures_v1_interface.name) == 0
                   && version >= ZWP_POINTER_GESTURES_V1_GET_HOLD_GESTURE_SINCE_VERSION) {
            self->m_pointerGestures = bind<zwp_pointer_gestures_v1>(
                registry, name, &zwp_pointer_gestures_v1_interface, version, 3);
        } else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
            self->m_dataDeviceManager = bind<wl_data_device_manager>(
                registry, name, &wl_data_device_manager_interface, version, 3);
        } else if (strcmp(interface, zwp_text_input_manager_v3_interface.name) == 0) {
            self->m_textInputManager = bind<zwp_text_input_manager_v3>(
                registry, name, &zwp_text_input_manager_v3_interface, version, 1);
        } else if (strcmp(interface, xdg_wm_dialog_v1_interface.name) == 0) {
            self->m_dialogManager = bind<xdg_wm_dialog_v1>(registry, name,
                                                          &xdg_wm_dialog_v1_interface, version, 1);
        } else if (strcmp(interface, cutefish_surface_blur_v1_interface.name) == 0) {
            self->m_surfaceBlur = bind<cutefish_surface_blur_v1>(
                registry, name, &cutefish_surface_blur_v1_interface, version, 1);
        } else if (strcmp(interface, cutefish_window_hints_v1_interface.name) == 0) {
            self->m_windowHints = bind<cutefish_window_hints_v1>(
                registry, name, &cutefish_window_hints_v1_interface, version, 1);
        } else if (strcmp(interface, wl_seat_interface.name) == 0 && !self->m_seat) {
            self->m_seat = bind<wl_seat>(registry, name, &wl_seat_interface, version, 8);
            wl_seat_add_listener(self->m_seat, &seat, self);
        }
    }

    static void globalRemove(void*, wl_registry*, uint32_t)
    {
    }

    static void ping(void*, xdg_wm_base* wmBase, uint32_t serial)
    {
        xdg_wm_base_pong(wmBase, serial);
    }

    static void capabilities(void* data, wl_seat* seat, uint32_t capabilities)
    {
        auto* self = static_cast<Connection*>(data);
        const bool hasPointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
        if (hasPointer && !self->m_pointer) {
            self->m_pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(self->m_pointer, &pointer, self);
            if (self->m_cursorShapeManager)
                self->m_cursorShapeDevice = wp_cursor_shape_manager_v1_get_pointer(
                    self->m_cursorShapeManager, self->m_pointer);
            if (self->m_pointerGestures) {
                self->m_hold = zwp_pointer_gestures_v1_get_hold_gesture(self->m_pointerGestures,
                                                                        self->m_pointer);
                zwp_pointer_gesture_hold_v1_add_listener(self->m_hold, &hold, self);
            }
        } else if (!hasPointer && self->m_pointer) {
            if (self->m_hold) {
                zwp_pointer_gesture_hold_v1_destroy(self->m_hold);
                self->m_hold = nullptr;
            }
            if (self->m_cursorShapeDevice) {
                wp_cursor_shape_device_v1_destroy(self->m_cursorShapeDevice);
                self->m_cursorShapeDevice = nullptr;
            }
            wl_pointer_release(self->m_pointer);
            self->m_pointer = nullptr;
            self->m_pointerFocus = nullptr;
        }

        const bool hasKeyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
        if (hasKeyboard && !self->m_keyboard) {
            self->m_keyboard = std::make_unique<Keyboard>(self->m_loop, wl_seat_get_keyboard(seat));
            self->m_keyboard->onKeyPressSerial = [self](uint32_t serial) {
                self->m_inputSerial = serial;
            };
        }
        else if (!hasKeyboard)
            self->m_keyboard.reset();
    }

    static void seatName(void*, wl_seat*, const char*)
    {
    }

    static void pointerEnter(void* data, wl_pointer*, uint32_t serial, wl_surface* surface,
                             wl_fixed_t x, wl_fixed_t y)
    {
        auto* self = static_cast<Connection*>(data);
        self->m_pointerFocus = shellSurfaceFor(surface);
        self->m_enterSerial = serial;
        // Without a cursor of its own the client's surface shows none.
        if (self->m_cursorShapeDevice)
            wp_cursor_shape_device_v1_set_shape(self->m_cursorShapeDevice, serial,
                                                WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
        if (self->m_pointerFocus)
            self->m_pointerFocus->delegate()->pointerEnter(wl_fixed_to_double(x),
                                                           wl_fixed_to_double(y));
    }

    static void pointerLeave(void* data, wl_pointer*, uint32_t, wl_surface*)
    {
        auto* self = static_cast<Connection*>(data);
        if (self->m_pointerFocus)
            self->m_pointerFocus->delegate()->pointerLeave();
        self->m_pointerFocus = nullptr;
    }

    static void pointerMotion(void* data, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y)
    {
        auto* self = static_cast<Connection*>(data);
        if (self->m_pointerFocus)
            self->m_pointerFocus->delegate()->pointerMotion(wl_fixed_to_double(x),
                                                            wl_fixed_to_double(y));
    }

    static void pointerButton(void* data, wl_pointer*, uint32_t serial, uint32_t, uint32_t button,
                              uint32_t state)
    {
        auto* self = static_cast<Connection*>(data);
        const bool pressed = state == WL_POINTER_BUTTON_STATE_PRESSED;
        if (pressed) {
            self->m_buttonSerial = serial;
            self->m_inputSerial = serial;
        }
        MouseButton mouseButton;
        switch (button) {
        case BTN_LEFT:
            mouseButton = MouseButton::Left;
            break;
        case BTN_RIGHT:
            mouseButton = MouseButton::Right;
            break;
        case BTN_MIDDLE:
            mouseButton = MouseButton::Middle;
            break;
        default:
            return;
        }
        const uint32_t modifiers = self->m_keyboard ? self->m_keyboard->modifiers() : NoModifier;
        if (self->m_pointerFocus)
            self->m_pointerFocus->delegate()->pointerButton(mouseButton, pressed, modifiers);
    }

    static void pointerAxis(void* data, wl_pointer*, uint32_t time, uint32_t axis, wl_fixed_t value)
    {
        auto* self = static_cast<Connection*>(data);
        self->m_scroll.time = time;
        if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
            self->m_scroll.deltaY += wl_fixed_to_double(value);
        else
            self->m_scroll.deltaX += wl_fixed_to_double(value);
        self->m_scrollPending = true;
    }

    static void pointerFrame(void* data, wl_pointer*)
    {
        auto* self = static_cast<Connection*>(data);
        if (!self->m_scrollPending)
            return;
        ScrollEvent event = self->m_scroll;
        event.modifiers = self->m_keyboard ? self->m_keyboard->modifiers() : NoModifier;
        self->m_scroll = {};
        self->m_scrollPending = false;
        if (self->m_pointerFocus)
            self->m_pointerFocus->delegate()->pointerScroll(event);
    }

    static void pointerAxisSource(void* data, wl_pointer*, uint32_t source)
    {
        auto* self = static_cast<Connection*>(data);
        self->m_scroll.precise = source == WL_POINTER_AXIS_SOURCE_FINGER
                || source == WL_POINTER_AXIS_SOURCE_CONTINUOUS;
    }

    static void pointerAxisStop(void* data, wl_pointer*, uint32_t time, uint32_t)
    {
        auto* self = static_cast<Connection*>(data);
        self->m_scroll.time = time;
        self->m_scroll.stop = true;
        self->m_scrollPending = true;
    }

    // Superseded by axis_value120 at the version bound.
    static void pointerAxisDiscrete(void*, wl_pointer*, uint32_t, int32_t)
    {
    }

    static void pointerAxisValue120(void* data, wl_pointer*, uint32_t axis, int32_t value120)
    {
        auto* self = static_cast<Connection*>(data);
        if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
            self->m_scroll.value120Y += value120;
        else
            self->m_scroll.value120X += value120;
        self->m_scrollPending = true;
    }

    static void holdBegin(void*, zwp_pointer_gesture_hold_v1*, uint32_t, uint32_t,
                          wl_surface* surface, uint32_t)
    {
        if (ShellSurface* shell = shellSurfaceFor(surface))
            shell->delegate()->pointerHold();
    }

    static void holdEnd(void*, zwp_pointer_gesture_hold_v1*, uint32_t, uint32_t, int32_t)
    {
    }

    static constexpr wl_registry_listener registry = {global, globalRemove};
    static constexpr zwp_pointer_gesture_hold_v1_listener hold = {holdBegin, holdEnd};
    static constexpr xdg_wm_base_listener wmBase = {ping};
    static constexpr wl_seat_listener seat = {capabilities, seatName};
    static constexpr wl_pointer_listener pointer = {
        pointerEnter,
        pointerLeave,
        pointerMotion,
        pointerButton,
        pointerAxis,
        pointerFrame,
        pointerAxisSource,
        pointerAxisStop,
        pointerAxisDiscrete,
        pointerAxisValue120,
        nullptr,
    };
};

std::unique_ptr<Connection> Connection::open(EventLoop& loop)
{
    wl_display* display = wl_display_connect(nullptr);
    if (!display) {
        fprintf(stderr, "cutefish-toolkit: cannot connect to the Wayland compositor\n");
        return nullptr;
    }
    std::unique_ptr<Connection> connection(new Connection(loop, display));
    if (!connection->initialize())
        return nullptr;
    return connection;
}

Connection::Connection(EventLoop& loop, wl_display* display)
    : m_loop(loop)
    , m_display(display)
{
}

Connection::~Connection()
{
    if (m_watch)
        m_loop.unwatch(m_watch);
    if (m_hook)
        m_loop.removePrepareHook(m_hook);
    m_textInput.reset();
    m_dataDevice.reset();
    if (m_textInputManager)
        zwp_text_input_manager_v3_destroy(m_textInputManager);
    if (m_dataDeviceManager)
        wl_data_device_manager_destroy(m_dataDeviceManager);
    m_keyboard.reset();
    if (m_hold)
        zwp_pointer_gesture_hold_v1_destroy(m_hold);
    if (m_pointerGestures)
        zwp_pointer_gestures_v1_destroy(m_pointerGestures);
    if (m_cursorShapeDevice)
        wp_cursor_shape_device_v1_destroy(m_cursorShapeDevice);
    if (m_pointer)
        wl_pointer_release(m_pointer);
    if (m_seat)
        wl_seat_release(m_seat);
    if (m_windowHints)
        cutefish_window_hints_v1_destroy(m_windowHints);
    if (m_surfaceBlur)
        cutefish_surface_blur_v1_destroy(m_surfaceBlur);
    if (m_dialogManager)
        xdg_wm_dialog_v1_destroy(m_dialogManager);
    if (m_cursorShapeManager)
        wp_cursor_shape_manager_v1_destroy(m_cursorShapeManager);
    if (m_presentation)
        wp_presentation_destroy(m_presentation);
    if (m_fractionalScaleManager)
        wp_fractional_scale_manager_v1_destroy(m_fractionalScaleManager);
    if (m_viewporter)
        wp_viewporter_destroy(m_viewporter);
    if (m_wmBase)
        xdg_wm_base_destroy(m_wmBase);
    if (m_compositor)
        wl_compositor_destroy(m_compositor);
    if (m_registry)
        wl_registry_destroy(m_registry);
    wl_display_flush(m_display);
    wl_display_disconnect(m_display);
}

bool Connection::initialize()
{
    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &ConnectionListeners::registry, this);
    // The first round trip binds the globals, the second brings the seat's
    // capabilities.
    wl_display_roundtrip(m_display);
    wl_display_roundtrip(m_display);

    if (!m_compositor || !m_wmBase || !m_viewporter || !m_fractionalScaleManager) {
        fprintf(stderr, "cutefish-toolkit: the compositor lacks a required global\n");
        return false;
    }
    if (m_seat && m_dataDeviceManager)
        m_dataDevice = std::make_unique<DataDevice>(*this, m_loop, m_dataDeviceManager, m_seat);
    if (m_seat && m_textInputManager)
        m_textInput = std::make_unique<TextInput>(m_textInputManager, m_seat);

    m_watch = m_loop.watch(wl_display_get_fd(m_display), EPOLLIN, [this](uint32_t) {
        if (wl_display_dispatch(m_display) < 0) {
            fprintf(stderr, "cutefish-toolkit: lost the Wayland connection\n");
            m_loop.quit(1);
        }
    });
    // Events another queue's reader left on ours would otherwise wait for
    // the next wake-up.
    m_hook = m_loop.addPrepareHook([this] {
        wl_display_dispatch_pending(m_display);
        wl_display_flush(m_display);
    });
    return true;
}

void Connection::setCursorShape(ShellSurface* shell, CursorShape shape)
{
    if (!m_cursorShapeDevice || shell != m_pointerFocus)
        return;
    uint32_t wire = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
    switch (shape) {
    case CursorShape::Default:
        break;
    case CursorShape::Pointer:
        wire = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER;
        break;
    case CursorShape::Text:
        wire = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT;
        break;
    case CursorShape::Move:
        wire = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE;
        break;
    case CursorShape::Grab:
        wire = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRAB;
        break;
    case CursorShape::Grabbing:
        wire = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING;
        break;
    case CursorShape::NotAllowed:
        wire = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED;
        break;
    case CursorShape::ResizeHorizontal:
        wire = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE;
        break;
    case CursorShape::ResizeVertical:
        wire = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE;
        break;
    }
    wp_cursor_shape_device_v1_set_shape(m_cursorShapeDevice, m_enterSerial, wire);
}

void Connection::forget(ShellSurface* shell)
{
    if (m_pointerFocus == shell)
        m_pointerFocus = nullptr;
    if (m_keyboard)
        m_keyboard->forget(shell);
    if (m_textInput)
        m_textInput->forget(shell);
}

void Connection::flush()
{
    wl_display_flush(m_display);
}

} // namespace cutefish::ui::wayland
