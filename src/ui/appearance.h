// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <systemd/sd-bus.h>

namespace cutefish::base::dbus {
class Connection;
}

namespace cutefish::ui {

// Keeps the theme in step with the user's appearance settings, which
// cutefish-services keeps: dark mode, accent colour, font and icon theme.
class AppearanceWatcher
{
public:
    // Reads the settings at once, then follows their change signals.
    explicit AppearanceWatcher(base::dbus::Connection& connection);
    ~AppearanceWatcher();

    AppearanceWatcher(const AppearanceWatcher&) = delete;
    AppearanceWatcher& operator=(const AppearanceWatcher&) = delete;

private:
    static int changed(sd_bus_message* message, void* data, sd_bus_error* error);
    static int replied(sd_bus_message* reply, void* data, sd_bus_error* error);
    static void apply(sd_bus_message* reply);

    base::dbus::Connection& m_connection;
    sd_bus_slot* m_match = nullptr;
    sd_bus_slot* m_call = nullptr;
};

} // namespace cutefish::ui
