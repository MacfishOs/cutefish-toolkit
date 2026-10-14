// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

struct sd_bus;

namespace cutefish::ui {

class EventLoop;

// The session bus through sd-bus, dispatched by the event loop.
class DBusConnection
{
public:
    // Null when there is no session bus.
    static std::unique_ptr<DBusConnection> session(EventLoop& loop);
    ~DBusConnection();

    DBusConnection(const DBusConnection&) = delete;
    DBusConnection& operator=(const DBusConnection&) = delete;

    sd_bus* bus() const { return m_bus; }

private:
    DBusConnection(EventLoop& loop, sd_bus* bus);
    void process();

    EventLoop& m_loop;
    sd_bus* m_bus;
    int m_watch = 0;
    int m_hook = 0;
};

} // namespace cutefish::ui
