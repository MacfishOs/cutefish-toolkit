// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cutefish::ui {

class AppearanceWatcher;
class DBusConnection;
class EventLoop;
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

    EventLoop& loop() const { return *m_loop; }
    // Wayland only.
    wayland::Connection& connection() const { return *m_connection; }
    gpu::EglContext& gpu() const { return *m_gpu; }
    TextContext& text() const { return *m_text; }

    // The session bus, when there is one.
    DBusConnection* sessionBus() const { return m_sessionBus.get(); }
    const std::vector<Window*>& windows() const { return m_windows; }

    int run();
    void quit(int exitCode = 0);

private:
    friend struct Theme;
    friend class Window;

    void themeChanged();

    std::vector<std::string> m_arguments;
    std::string m_applicationId;
    Platform m_platform;
    std::unique_ptr<EventLoop> m_loop;
    std::unique_ptr<wayland::Connection> m_connection;
    std::unique_ptr<gpu::EglContext> m_gpu;
    std::unique_ptr<TextContext> m_text;
    std::unique_ptr<DBusConnection> m_sessionBus;
    std::unique_ptr<AppearanceWatcher> m_appearance;
    std::vector<Window*> m_windows;
};

} // namespace cutefish::ui
