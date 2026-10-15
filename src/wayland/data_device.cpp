// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wayland/data_device.h"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <sys/epoll.h>
#include <unistd.h>
#include <wayland-client.h>

#include "base/event_loop.h"
#include "wayland/connection.h"

namespace cutefish::ui::wayland {

// In order of preference.
static const char* const kTextTypes[] = {
    "text/plain;charset=utf-8",
    "text/plain",
    "UTF8_STRING",
    "STRING",
    "TEXT",
};

struct DataDeviceListeners
{
    static void offerMimeType(void* data, wl_data_offer* offer, const char* type)
    {
        auto* self = static_cast<DataDevice*>(data);
        self->m_offers[offer].push_back(type);
    }

    static void offerSourceActions(void*, wl_data_offer*, uint32_t)
    {
    }

    static void offerAction(void*, wl_data_offer*, uint32_t)
    {
    }

    static void dataOffer(void* data, wl_data_device*, wl_data_offer* offer)
    {
        auto* self = static_cast<DataDevice*>(data);
        self->m_offers[offer];
        wl_data_offer_add_listener(offer, &DataDeviceListeners::offer, self);
    }

    static void enter(void* data, wl_data_device*, uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t,
                      wl_data_offer* offer)
    {
        // Nothing is accepted yet, so a drop never comes.
        auto* self = static_cast<DataDevice*>(data);
        if (offer)
            self->dropOffer(offer);
    }

    static void leave(void*, wl_data_device*)
    {
    }

    static void motion(void*, wl_data_device*, uint32_t, wl_fixed_t, wl_fixed_t)
    {
    }

    static void drop(void*, wl_data_device*)
    {
    }

    static void selection(void* data, wl_data_device*, wl_data_offer* offer)
    {
        auto* self = static_cast<DataDevice*>(data);
        if (self->m_selection && self->m_selection != offer)
            self->dropOffer(self->m_selection);
        self->m_selection = offer;
    }

    static void sourceTarget(void*, wl_data_source*, const char*)
    {
    }

    static void sourceSend(void* data, wl_data_source*, const char*, int32_t fd)
    {
        auto* self = static_cast<DataDevice*>(data);
        // Readers drain the pipe as it fills; clipboard text is short.
        const int flags = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        const char* text = self->m_sourceText.data();
        size_t left = self->m_sourceText.size();
        while (left > 0) {
            const ssize_t written = write(fd, text, left);
            if (written <= 0)
                break;
            text += written;
            left -= size_t(written);
        }
        close(fd);
    }

    static void sourceCancelled(void* data, wl_data_source* source)
    {
        auto* self = static_cast<DataDevice*>(data);
        wl_data_source_destroy(source);
        if (self->m_source == source) {
            self->m_source = nullptr;
            self->m_sourceText.clear();
        }
    }

    static void sourceDropPerformed(void*, wl_data_source*)
    {
    }

    static void sourceFinished(void*, wl_data_source*)
    {
    }

    static void sourceAction(void*, wl_data_source*, uint32_t)
    {
    }

    static constexpr wl_data_offer_listener offer = {
        offerMimeType,
        offerSourceActions,
        offerAction,
    };
    static constexpr wl_data_device_listener device = {
        dataOffer, enter, leave, motion, drop, selection,
    };
    static constexpr wl_data_source_listener source = {
        sourceTarget, sourceSend, sourceCancelled, sourceDropPerformed, sourceFinished, sourceAction,
    };
};

DataDevice::DataDevice(Connection& connection, base::EventLoop& loop, wl_data_device_manager* manager,
                       wl_seat* seat)
    : m_connection(connection)
    , m_loop(loop)
    , m_manager(manager)
    , m_device(wl_data_device_manager_get_data_device(manager, seat))
{
    wl_data_device_add_listener(m_device, &DataDeviceListeners::device, this);
}

DataDevice::~DataDevice()
{
    if (m_source)
        wl_data_source_destroy(m_source);
    for (const auto& [offer, types] : m_offers)
        wl_data_offer_destroy(offer);
    wl_data_device_release(m_device);
}

void DataDevice::dropOffer(wl_data_offer* offer)
{
    m_offers.erase(offer);
    wl_data_offer_destroy(offer);
}

void DataDevice::setSelection(const std::string& text)
{
    if (m_source)
        wl_data_source_destroy(m_source);
    m_source = wl_data_device_manager_create_data_source(m_manager);
    m_sourceText = text;
    wl_data_source_add_listener(m_source, &DataDeviceListeners::source, this);
    for (const char* type : kTextTypes)
        wl_data_source_offer(m_source, type);
    wl_data_device_set_selection(m_device, m_source, m_connection.inputSerial());
}

void DataDevice::requestSelection(std::function<void(std::string)> callback)
{
    // Our own selection needs no round trip through the compositor.
    if (m_source) {
        m_loop.post([callback = std::move(callback), text = m_sourceText] { callback(text); });
        return;
    }
    const auto it = m_selection ? m_offers.find(m_selection) : m_offers.end();
    const char* type = nullptr;
    if (it != m_offers.end()) {
        for (const char* candidate : kTextTypes) {
            if (std::find(it->second.begin(), it->second.end(), candidate) != it->second.end()) {
                type = candidate;
                break;
            }
        }
    }
    int fds[2];
    if (!type || pipe2(fds, O_CLOEXEC | O_NONBLOCK) < 0) {
        m_loop.post([callback = std::move(callback)] { callback({}); });
        return;
    }

    wl_data_offer_receive(m_selection, type, fds[1]);
    close(fds[1]);
    m_connection.flush();

    struct Read
    {
        int fd;
        int watch = 0;
        std::string text;
        std::function<void(std::string)> callback;
    };
    auto read = std::make_shared<Read>();
    read->fd = fds[0];
    read->callback = std::move(callback);
    read->watch = m_loop.watch(fds[0], EPOLLIN, [this, read](uint32_t) {
        char buffer[4096];
        for (;;) {
            const ssize_t count = ::read(read->fd, buffer, sizeof(buffer));
            if (count > 0) {
                read->text.append(buffer, size_t(count));
                continue;
            }
            if (count < 0 && errno == EAGAIN)
                return;
            break;
        }
        m_loop.unwatch(read->watch);
        close(read->fd);
        read->callback(std::move(read->text));
    });
}

} // namespace cutefish::ui::wayland
