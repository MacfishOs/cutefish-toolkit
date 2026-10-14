// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/appearance.h"

#include <cstring>
#include <string>
#include <systemd/sd-bus.h>

#include "base/dbus_connection.h"
#include "ui/theme.h"

namespace cutefish::ui {

static constexpr const char* kService = "com.cutefish.Services";
static constexpr const char* kPath = "/com/cutefish/Services/Appearance";
static constexpr const char* kInterface = "com.cutefish.Services.Appearance";

// FishUI's accents, in the order of the setting's index.
static const Color kAccents[] = {
    accent::blue, accent::red, accent::green, accent::purple, accent::pink, accent::orange, accent::grey,
};

AppearanceWatcher::AppearanceWatcher(DBusConnection& connection)
    : m_connection(connection)
{
    sd_bus* bus = connection.bus();
    // One blocking read at start-up, so the first frame has the right colours.
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    if (sd_bus_call_method(bus, kService, kPath, "org.freedesktop.DBus.Properties", "GetAll", &error,
                           &reply, "s", kInterface) >= 0) {
        apply(reply);
        sd_bus_message_unref(reply);
    }
    sd_bus_error_free(&error);

    sd_bus_match_signal(bus, &m_match, kService, kPath, kInterface, nullptr,
                        &AppearanceWatcher::changed, this);
}

AppearanceWatcher::~AppearanceWatcher()
{
    sd_bus_slot_unref(m_call);
    sd_bus_slot_unref(m_match);
}

int AppearanceWatcher::changed(sd_bus_message* message, void* data, sd_bus_error*)
{
    auto* self = static_cast<AppearanceWatcher*>(data);
    const char* member = sd_bus_message_get_member(message);
    // Wallpaper and cursor signals change nothing the toolkit draws.
    if (!member || std::strstr(member, "allpaper") || std::strstr(member, "ursor")
        || std::strstr(member, "ackground"))
        return 0;
    sd_bus_slot_unref(self->m_call);
    self->m_call = nullptr;
    sd_bus_call_method_async(self->m_connection.bus(), &self->m_call, kService, kPath,
                             "org.freedesktop.DBus.Properties", "GetAll",
                             &AppearanceWatcher::replied, self, "s", kInterface);
    return 0;
}

int AppearanceWatcher::replied(sd_bus_message* reply, void* data, sd_bus_error*)
{
    auto* self = static_cast<AppearanceWatcher*>(data);
    if (!sd_bus_message_is_method_error(reply, nullptr))
        apply(reply);
    sd_bus_slot_unref(self->m_call);
    self->m_call = nullptr;
    return 0;
}

void AppearanceWatcher::apply(sd_bus_message* reply)
{
    bool dark = Theme::current().darkMode;
    int accent = 0;
    double pointSize = Theme::current().fontPointSize;
    std::string family = Theme::current().fontFamily;
    std::string iconTheme = Theme::current().iconTheme;
    bool continuous = Theme::current().continuousCorners;

    if (sd_bus_message_enter_container(reply, 'a', "{sv}") < 0)
        return;
    while (sd_bus_message_enter_container(reply, 'e', "sv") > 0) {
        const char* name = nullptr;
        sd_bus_message_read(reply, "s", &name);
        const std::string key = name ? name : "";
        const char* contents = nullptr;
        sd_bus_message_peek_type(reply, nullptr, &contents);
        if (sd_bus_message_enter_container(reply, 'v', contents) < 0)
            break;
        const std::string type = contents ? contents : "";
        if (type == "b") {
            int value = 0;
            sd_bus_message_read(reply, "b", &value);
            if (key == "isDarkMode")
                dark = value;
            else if (key == "continuousCorners")
                continuous = value;
        } else if (type == "i") {
            int value = 0;
            sd_bus_message_read(reply, "i", &value);
            if (key == "accentColor")
                accent = value;
        } else if (type == "d") {
            double value = 0;
            sd_bus_message_read(reply, "d", &value);
            if (key == "systemFontPointSize" && value > 0)
                pointSize = value;
        } else if (type == "s") {
            const char* value = nullptr;
            sd_bus_message_read(reply, "s", &value);
            if (key == "systemFont" && value)
                family = value;
            else if (key == "iconTheme" && value && *value)
                iconTheme = value;
        } else {
            sd_bus_message_skip(reply, contents);
        }
        sd_bus_message_exit_container(reply);
        sd_bus_message_exit_container(reply);
    }
    sd_bus_message_exit_container(reply);

    Theme theme = dark ? Theme::dark() : Theme::light();
    if (accent >= 0 && accent < int(std::size(kAccents)))
        theme.highlight = kAccents[accent];
    theme.fontPointSize = pointSize;
    theme.fontFamily = family;
    theme.iconTheme = iconTheme;
    theme.continuousCorners = continuous;
    Theme::setCurrent(theme);
}

} // namespace cutefish::ui
