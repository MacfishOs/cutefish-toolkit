// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct wl_data_device;
struct wl_data_device_manager;
struct wl_data_offer;
struct wl_data_source;
struct wl_seat;

namespace cutefish::base {
class EventLoop;
}

namespace cutefish::ui::wayland {

class Connection;
struct DataDeviceListeners;

// The seat's clipboard selection, as text. Drag and drop is not taken yet.
class DataDevice
{
public:
    DataDevice(Connection& connection, base::EventLoop& loop, wl_data_device_manager* manager,
               wl_seat* seat);
    ~DataDevice();

    DataDevice(const DataDevice&) = delete;
    DataDevice& operator=(const DataDevice&) = delete;

    void setSelection(const std::string& text);
    void requestSelection(std::function<void(std::string)> callback);

private:
    friend struct DataDeviceListeners;

    void dropOffer(wl_data_offer* offer);

    Connection& m_connection;
    base::EventLoop& m_loop;
    wl_data_device_manager* m_manager;
    wl_data_device* m_device = nullptr;
    // Announced offers and their MIME types, until they become the selection or go.
    std::unordered_map<wl_data_offer*, std::vector<std::string>> m_offers;
    wl_data_offer* m_selection = nullptr;
    wl_data_source* m_source = nullptr;
    std::string m_sourceText;
};

} // namespace cutefish::ui::wayland
