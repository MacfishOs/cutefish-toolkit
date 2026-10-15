// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/application.h"

#include <clocale>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <utility>

#include "base/dbus.h"
#include "base/event_loop.h"
#include "gpu/egl_context.h"
#include "ui/appearance.h"
#include "ui/platform_window.h"
#include "ui/text_context.h"
#include "ui/window.h"
#include "wayland/connection.h"

namespace cutefish::ui {

static Application* s_instance = nullptr;

Application::Application(int argc, char** argv, Platform platform)
    : m_arguments(argv, argv + argc)
    , m_platform(platform)
{
    s_instance = this;
    std::setlocale(LC_ALL, "");
    std::error_code error;
    m_applicationId = std::filesystem::read_symlink("/proc/self/exe", error).filename();

    m_loop = platform == Platform::Embedded ? base::EventLoop::current() : nullptr;
    if (!m_loop) {
        m_ownedLoop = std::make_unique<base::EventLoop>();
        m_loop = m_ownedLoop.get();
    }
    m_text = std::make_unique<TextContext>();
    m_text->setHintedLayout(platform != Platform::Embedded);
    if (platform != Platform::Wayland)
        return;

    m_connection = wayland::Connection::open(*m_loop);
    if (!m_connection)
        std::exit(1);
    m_gpu = gpu::EglContext::create(m_connection->display());
    if (!m_gpu)
        std::exit(1);

    m_sessionBus = base::dbus::Connection::session(m_loop);
    if (m_sessionBus)
        m_appearance = std::make_unique<AppearanceWatcher>(*m_sessionBus);
}

Application::~Application()
{
    // Posted tasks may own windows, whose views still need the loop, the
    // connection and the GPU context while they are destroyed. A host's loop
    // keeps its tasks.
    if (m_ownedLoop)
        m_loop->dropPosted();
    m_appearance.reset();
    m_sessionBus = nullptr;
    m_text.reset();
    m_gpu.reset();
    m_connection.reset();
    m_ownedLoop.reset();
    m_loop = nullptr;
    s_instance = nullptr;
}

Application* Application::instance()
{
    return s_instance;
}

int Application::run()
{
    return m_loop->run();
}

void Application::setApplicationId(std::string applicationId)
{
    m_applicationId = std::move(applicationId);
    for (Window* window : m_windows) {
        if (!window->m_parent)
            window->m_platform->setAppId(m_applicationId);
    }
}

void Application::themeChanged()
{
    for (Window* window : m_windows)
        window->handleThemeChanged();
}

void Application::scheduleActiveUpdate()
{
    if (m_activeUpdatePending)
        return;
    m_activeUpdatePending = true;
    const auto update = [this] {
        m_activeUpdatePending = false;
        for (Window* window : std::vector<Window*>(m_windows))
            window->updateActive();
    };
    // Headless windows have no compositor to wait for.
    if (m_connection)
        m_connection->sync(update);
    else
        update();
}

void Application::quit(int exitCode)
{
    m_loop->quit(exitCode);
}

} // namespace cutefish::ui
