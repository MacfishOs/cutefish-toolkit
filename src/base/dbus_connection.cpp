// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "base/dbus_connection.h"

#include <sys/epoll.h>
#include <systemd/sd-bus.h>

#include "base/event_loop.h"

namespace cutefish::ui {

std::unique_ptr<DBusConnection> DBusConnection::session(EventLoop& loop)
{
    sd_bus* bus = nullptr;
    if (sd_bus_open_user(&bus) < 0)
        return nullptr;
    return std::unique_ptr<DBusConnection>(new DBusConnection(loop, bus));
}

DBusConnection::DBusConnection(EventLoop& loop, sd_bus* bus)
    : m_loop(loop)
    , m_bus(bus)
{
    m_watch = m_loop.watch(sd_bus_get_fd(bus), EPOLLIN, [this](uint32_t) { process(); });
    // Messages read during a blocking call wait in the queue, and writes
    // may be pending; neither wakes the loop on its own.
    m_hook = m_loop.addPrepareHook([this] {
        process();
        sd_bus_flush(m_bus);
    });
}

DBusConnection::~DBusConnection()
{
    m_loop.unwatch(m_watch);
    m_loop.removePrepareHook(m_hook);
    sd_bus_flush_close_unref(m_bus);
    m_bus = nullptr;
}

void DBusConnection::process()
{
    while (m_bus && sd_bus_process(m_bus, nullptr) > 0) {
    }
}

} // namespace cutefish::ui
