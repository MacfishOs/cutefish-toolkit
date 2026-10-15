// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ui/event.h"

struct cutefish_surface_blur_v1;
struct cutefish_window_hints_v1;
struct wl_compositor;
struct wl_data_device_manager;
struct wl_display;
struct wl_output;
struct wl_pointer;
struct wl_registry;
struct wl_seat;
struct wl_surface;
struct wp_cursor_shape_device_v1;
struct wp_cursor_shape_manager_v1;
struct wp_fractional_scale_manager_v1;
struct wp_presentation;
struct wp_viewporter;
struct xdg_activation_token_v1;
struct xdg_activation_v1;
struct xdg_wm_base;
struct zxdg_exported_v2;
struct zxdg_exporter_v2;
struct xdg_wm_dialog_v1;
struct zwp_pointer_gesture_hold_v1;
struct zwp_pointer_gestures_v1;
struct zwp_text_input_manager_v3;
struct zxdg_output_manager_v1;
struct zxdg_output_v1;

namespace cutefish::base {
class EventLoop;
}

namespace cutefish::ui::wayland {

class DataDevice;
class Keyboard;
class TextInput;
class ShellSurface;
struct ConnectionListeners;

// The connection to the compositor: its globals, the seat, and the routing
// of input to the window whose surface it is for.
class Connection
{
public:
    static std::unique_ptr<Connection> open(base::EventLoop& loop);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    wl_display* display() const { return m_display; }
    wl_compositor* compositor() const { return m_compositor; }
    xdg_wm_base* wmBase() const { return m_wmBase; }
    wp_viewporter* viewporter() const { return m_viewporter; }
    wp_fractional_scale_manager_v1* fractionalScaleManager() const { return m_fractionalScaleManager; }
    wp_presentation* presentation() const { return m_presentation; }
    cutefish_window_hints_v1* windowHints() const { return m_windowHints; }
    cutefish_surface_blur_v1* surfaceBlur() const { return m_surfaceBlur; }
    xdg_wm_dialog_v1* dialogManager() const { return m_dialogManager; }
    wl_seat* seat() const { return m_seat; }

    // The serial of the last button press, for move and resize requests.
    uint32_t buttonSerial() const { return m_buttonSerial; }
    // The serial of the last key or button press, for the clipboard.
    uint32_t inputSerial() const { return m_inputSerial; }

    DataDevice* dataDevice() const { return m_dataDevice.get(); }

    // Runs once the compositor has answered everything sent so far, as Qt's
    // requestWaylandSync, so a batch of focus changes reads as one.
    void sync(std::function<void()> done);

    // A handle another client can parent its windows to, as
    // "wayland:<handle>", once the compositor gave one; kept while the
    // window lives.
    void exportToplevel(ShellSurface* shell, std::function<void(const std::string& handle)> done);

    // xdg-activation for the window, as Qt's requestActivate asks it: a token
    // for the last input and the focused window, then the activation.
    void activate(ShellSurface* shell, const std::string& appId);
    TextInput* textInput() const { return m_textInput.get(); }

    // Applies while the pointer is over the window's surface.
    void setCursorShape(ShellSurface* shell, CursorShape shape);
    // A window going away loses pointer and keyboard focus.
    void forget(ShellSurface* shell);

    void flush();

    // An output's logical size, as xdg-output gives it and Qt takes for the
    // screen's geometry; empty until known.
    SizeF outputSize(wl_output* output) const;
    // The first output's, for a window that is on none yet.
    SizeF primaryOutputSize() const;

    // The token for an activation came; for the listener.
    void activationDone(xdg_activation_token_v1* token, const char* tokenString);
    // The compositor named an exported window; for the listener.
    void exported(zxdg_exported_v2* exported, const char* handle);

private:
    friend struct ConnectionListeners;

    Connection(base::EventLoop& loop, wl_display* display);
    bool initialize();

    base::EventLoop& m_loop;
    wl_display* m_display = nullptr;
    wl_registry* m_registry = nullptr;
    wl_compositor* m_compositor = nullptr;
    xdg_wm_base* m_wmBase = nullptr;
    wp_viewporter* m_viewporter = nullptr;
    wp_fractional_scale_manager_v1* m_fractionalScaleManager = nullptr;
    wp_presentation* m_presentation = nullptr;
    wp_cursor_shape_manager_v1* m_cursorShapeManager = nullptr;
    cutefish_window_hints_v1* m_windowHints = nullptr;
    cutefish_surface_blur_v1* m_surfaceBlur = nullptr;
    xdg_wm_dialog_v1* m_dialogManager = nullptr;
    xdg_activation_v1* m_activation = nullptr;
    // Tokens on their way, and the window each is for.
    std::vector<std::pair<xdg_activation_token_v1*, ShellSurface*>> m_activations;
    zxdg_exporter_v2* m_exporter = nullptr;
    struct Export
    {
        ShellSurface* shell;
        zxdg_exported_v2* exported;
        std::string handle;
        std::vector<std::function<void(const std::string&)>> waiting;
    };
    std::vector<Export> m_exports;
    zxdg_output_manager_v1* m_outputManager = nullptr;
    struct Output
    {
        uint32_t name;
        wl_output* output;
        zxdg_output_v1* xdgOutput;
        SizeF logicalSize;
    };
    std::vector<Output> m_outputs;
    void watchOutput(Output& output);
    static void releaseOutput(const Output& output);
    wl_seat* m_seat = nullptr;
    wl_pointer* m_pointer = nullptr;
    wp_cursor_shape_device_v1* m_cursorShapeDevice = nullptr;
    zwp_pointer_gestures_v1* m_pointerGestures = nullptr;
    zwp_pointer_gesture_hold_v1* m_hold = nullptr;
    std::unique_ptr<Keyboard> m_keyboard;
    wl_data_device_manager* m_dataDeviceManager = nullptr;
    std::unique_ptr<DataDevice> m_dataDevice;
    zwp_text_input_manager_v3* m_textInputManager = nullptr;
    std::unique_ptr<TextInput> m_textInput;
    ShellSurface* m_pointerFocus = nullptr;
    uint32_t m_enterSerial = 0;
    uint32_t m_buttonSerial = 0;
    uint32_t m_inputSerial = 0;
    // Axis events gather until the pointer frame that ends them.
    ScrollEvent m_scroll;
    bool m_scrollPending = false;
    int m_watch = 0;
    int m_hook = 0;
};

} // namespace cutefish::ui::wayland
