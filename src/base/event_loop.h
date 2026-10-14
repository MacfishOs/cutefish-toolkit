// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace cutefish::ui {

// Dispatches file descriptors, timers and posted tasks on the thread that
// calls run(). Everything except post() belongs to that thread.
class EventLoop
{
public:
    using Task = std::function<void()>;
    using FdCallback = std::function<void(uint32_t events)>;

    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // Returns an id for unwatch(), or 0 when the descriptor cannot be watched.
    int watch(int fd, uint32_t events, FdCallback callback);
    void unwatch(int id);

    int startTimer(std::chrono::nanoseconds interval, bool repeat, Task task);
    void stopTimer(int id);

    // Runs the task on the loop's next iteration; callable from any thread.
    void post(Task task);

    // Runs before every wait, e.g. to flush a connection.
    int addPrepareHook(Task hook);
    void removePrepareHook(int id);

    int run();
    void quit(int exitCode = 0);

private:
    struct Watch
    {
        int fd = -1;
        bool ownsFd = false;
        FdCallback callback;
    };

    void runPosted();
    bool hasPosted();

    int m_epoll = -1;
    int m_wakeFd = -1;
    int m_nextId = 1;
    bool m_running = false;
    int m_exitCode = 0;
    // Shared so a callback can unwatch itself while it runs.
    std::unordered_map<int, std::shared_ptr<Watch>> m_watches;
    std::vector<std::pair<int, Task>> m_prepareHooks;
    std::mutex m_postedMutex;
    std::vector<Task> m_posted;
};

} // namespace cutefish::ui
