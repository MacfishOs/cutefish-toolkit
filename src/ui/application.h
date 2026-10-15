// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cutefish::base {
class EventLoop;
}

namespace cutefish::base::dbus {
class Connection;
}

namespace cutefish::ui {

class AppearanceWatcher;
class TextContext;
class Window;

namespace gpu {
class EglContext;
}

namespace wayland {
class Connection;
}

// The process-wide state windows share: the event loop, the compositor
// connection, the GPU context and the fonts. Exits when the compositor
// cannot be reached.
class Application
{
public:
    enum class Platform {
        Wayland,
        // No compositor and no GPU: windows render only when grabbed, for tests.
        Headless,
        // Inside a process that owns the screen, such as the compositor:
        // EmbeddedWindows render into memory when the host asks, input comes
        // from the host, and nothing connects to Wayland, a GPU or D-Bus.
        // The application uses the calling thread's current loop when there
        // is one, and leaves it to the host; otherwise the host drives
        // loop() through its descriptor.
        Embedded,
    };

    Application(int argc, char** argv, Platform platform = Platform::Wayland);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    static Application* instance();

    const std::vector<std::string>& arguments() const { return m_arguments; }
    Platform platform() const { return m_platform; }

    // Names the application's desktop entry; every toplevel window carries it.
    // Defaults to the executable's name.
    void setApplicationId(std::string applicationId);
    const std::string& applicationId() const { return m_applicationId; }

    base::EventLoop& loop() const { return *m_loop; }
    bool hasConnection() const { return m_connection != nullptr; }
    // Wayland only.
    wayland::Connection& connection() const { return *m_connection; }
    gpu::EglContext& gpu() const { return *m_gpu; }
    TextContext& text() const { return *m_text; }

    // The loop's shared session bus connection, when there is one.
    base::dbus::Connection* sessionBus() const { return m_sessionBus; }
    const std::vector<Window*>& windows() const { return m_windows; }

    int run();
    void quit(int exitCode = 0);

private:
    friend class EmbeddedWindow;
    friend struct Theme;
    friend class Window;

    void themeChanged();
    // Notifies the windows whose isActive() changed, after the events at hand.
    void scheduleActiveUpdate();

    std::vector<std::string> m_arguments;
    std::string m_applicationId;
    Platform m_platform;
    std::unique_ptr<base::EventLoop> m_ownedLoop;
    base::EventLoop* m_loop = nullptr;
    std::unique_ptr<wayland::Connection> m_connection;
    std::unique_ptr<gpu::EglContext> m_gpu;
    std::unique_ptr<TextContext> m_text;
    base::dbus::Connection* m_sessionBus = nullptr;
    std::unique_ptr<AppearanceWatcher> m_appearance;
    std::vector<Window*> m_windows;
    bool m_activeUpdatePending = false;
};

} // namespace cutefish::ui
