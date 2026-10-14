// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/clipboard.h"

#include "ui/application.h"
#include "wayland/connection.h"
#include "wayland/data_device.h"

namespace cutefish::ui {

// Headless applications share a clipboard within the process.
static std::string s_headlessText;

void Clipboard::setText(const std::string& text)
{
    Application* application = Application::instance();
    if (application->platform() == Application::Platform::Headless) {
        s_headlessText = text;
        return;
    }
    if (auto* device = application->connection().dataDevice())
        device->setSelection(text);
}

void Clipboard::requestText(std::function<void(std::string text)> callback)
{
    Application* application = Application::instance();
    if (application->platform() == Application::Platform::Headless) {
        callback(s_headlessText);
        return;
    }
    if (auto* device = application->connection().dataDevice())
        device->requestSelection(std::move(callback));
    else
        callback({});
}

} // namespace cutefish::ui
