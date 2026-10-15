// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "base/file_watcher.h"

#include <filesystem>
#include <map>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "base/event_loop.h"

namespace cutefish::base {

static constexpr uint32_t kMask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE | IN_ATTRIB
    | IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR;

struct FileWatch::Inotify
{
    struct Watcher
    {
        int descriptor;
        // Empty for a directory watch.
        std::string name;
        std::function<void(const std::string& name)> callback;
    };

    explicit Inotify(EventLoop& loop);
    ~Inotify();

    int add(const std::string& directory, std::string name, std::function<void(const std::string&)> callback);
    void remove(int id);
    void read();

    // Null once the loop is gone.
    EventLoop* loop;
    int fd = -1;
    int watch = 0;
    int nextId = 1;
    std::map<int, Watcher> watchers;
    // Watchers per inotify descriptor; inotify gives one per directory.
    std::map<int, int> users;
};

FileWatch::Inotify::Inotify(EventLoop& eventLoop)
    : loop(&eventLoop)
{
    fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd >= 0)
        watch = loop->watch(fd, EPOLLIN, [this](uint32_t) { read(); });
}

FileWatch::Inotify::~Inotify()
{
    if (loop && watch)
        loop->unwatch(watch);
    if (fd >= 0)
        close(fd);
}

int FileWatch::Inotify::add(const std::string& directory, std::string name,
                            std::function<void(const std::string&)> callback)
{
    if (fd < 0)
        return 0;
    const int descriptor = inotify_add_watch(fd, directory.c_str(), kMask);
    if (descriptor < 0)
        return 0;
    ++users[descriptor];
    const int id = nextId++;
    watchers.emplace(id, Watcher{descriptor, std::move(name), std::move(callback)});
    return id;
}

void FileWatch::Inotify::remove(int id)
{
    const auto it = watchers.find(id);
    if (it == watchers.end())
        return;
    const int descriptor = it->second.descriptor;
    watchers.erase(it);
    if (--users[descriptor] == 0) {
        users.erase(descriptor);
        inotify_rm_watch(fd, descriptor);
    }
}

void FileWatch::Inotify::read()
{
    alignas(inotify_event) char buffer[8192];
    while (true) {
        const ssize_t size = ::read(fd, buffer, sizeof(buffer));
        if (size <= 0)
            return;

        // Callbacks may add and remove watchers, so each event looks them up again.
        for (ssize_t offset = 0; offset < size;) {
            const auto* event = reinterpret_cast<const inotify_event*>(buffer + offset);
            offset += ssize_t(sizeof(inotify_event) + event->len);
            const std::string name = event->len ? std::string(event->name) : std::string();
            const bool self = event->mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_IGNORED);

            std::vector<int> ids;
            for (const auto& [id, watcher] : watchers) {
                if (watcher.descriptor != event->wd)
                    continue;
                if (watcher.name.empty() || self || watcher.name == name)
                    ids.push_back(id);
            }
            for (const int id : ids) {
                const auto it = watchers.find(id);
                if (it == watchers.end())
                    continue;
                // The callback may remove its own watcher.
                auto callback = it->second.callback;
                callback(self ? std::string() : name);
            }
        }
    }
}

FileWatch::~FileWatch()
{
    reset();
}

FileWatch::FileWatch(FileWatch&& other) noexcept
    : m_inotify(std::move(other.m_inotify))
    , m_id(std::exchange(other.m_id, 0))
{
}

FileWatch& FileWatch::operator=(FileWatch&& other) noexcept
{
    if (this != &other) {
        reset();
        m_inotify = std::move(other.m_inotify);
        m_id = std::exchange(other.m_id, 0);
    }
    return *this;
}

void FileWatch::reset()
{
    if (m_inotify && m_id)
        m_inotify->remove(m_id);
    m_inotify.reset();
    m_id = 0;
}

std::shared_ptr<FileWatch::Inotify> FileWatch::inotify(EventLoop* loop)
{
    // The instance shared per loop, on the loop's thread.
    static thread_local std::map<EventLoop*, std::weak_ptr<Inotify>> s_instances;

    if (!loop)
        loop = EventLoop::current();
    if (!loop)
        return nullptr;
    if (auto existing = s_instances[loop].lock())
        return existing;
    auto created = std::make_shared<Inotify>(*loop);
    s_instances[loop] = created;
    loop->addDestroyHook([loop, weak = std::weak_ptr<Inotify>(created)] {
        if (auto inotify = weak.lock()) {
            loop->unwatch(inotify->watch);
            inotify->loop = nullptr;
        }
        s_instances.erase(loop);
    });
    return created;
}

FileWatch FileWatch::file(const std::string& path, std::function<void()> changed, EventLoop* loop)
{
    FileWatch result;
    result.m_inotify = inotify(loop);
    if (!result.m_inotify)
        return {};
    const std::filesystem::path file(path);
    std::string directory = file.parent_path().string();
    if (directory.empty())
        directory = ".";
    result.m_id = result.m_inotify->add(directory, file.filename().string(),
                                        [changed = std::move(changed)](const std::string&) { changed(); });
    if (!result.m_id)
        result.m_inotify.reset();
    return result;
}

FileWatch FileWatch::directory(const std::string& path, std::function<void(const std::string& name)> changed,
                               EventLoop* loop)
{
    FileWatch result;
    result.m_inotify = inotify(loop);
    if (!result.m_inotify)
        return {};
    result.m_id = result.m_inotify->add(path, std::string(), std::move(changed));
    if (!result.m_id)
        result.m_inotify.reset();
    return result;
}

} // namespace cutefish::base
