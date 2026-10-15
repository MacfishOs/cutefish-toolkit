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

namespace cutefish::base {

class EventLoop;

// Lets another thread post to a loop that may be gone by the time it does;
// what it posts then is dropped.
class LoopHandle
{
public:
    LoopHandle() = default;

    // Returns false when the loop is gone.
    bool post(std::function<void()> task) const;

private:
    friend class EventLoop;
    struct Link
    {
        std::mutex mutex;
        EventLoop* loop = nullptr;
    };

    std::shared_ptr<Link> m_link;
};

// Dispatches file descriptors, timers and posted tasks on the thread that
// calls run(). Everything except post() and handle() belongs to that thread.
class EventLoop
{
public:
    using Task = std::function<void()>;
    using FdCallback = std::function<void(uint32_t events)>;

    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // The loop created last on the calling thread that still exists, or null.
    static EventLoop* current();

    LoopHandle handle() const;

    // Returns an id for unwatch(), or 0 when the descriptor cannot be watched.
    int watch(int fd, uint32_t events, FdCallback callback);
    // Changes the epoll events a watch waits for.
    void setWatchEvents(int id, uint32_t events);
    void unwatch(int id);

    int startTimer(std::chrono::nanoseconds interval, bool repeat, Task task);
    void stopTimer(int id);

    // Runs the task on the loop's next iteration; callable from any thread.
    void post(Task task);
    // Destroys the tasks posted but not run yet, without running them.
    void dropPosted();

    // Runs first thing when the loop is destroyed, for what lives as long as
    // the loop does.
    void addDestroyHook(Task hook);

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

    EventLoop* m_previous = nullptr;
    LoopHandle m_handle;
    int m_epoll = -1;
    int m_wakeFd = -1;
    int m_nextId = 1;
    bool m_running = false;
    int m_exitCode = 0;
    // Shared so a callback can unwatch itself while it runs.
    std::unordered_map<int, std::shared_ptr<Watch>> m_watches;
    std::vector<std::pair<int, Task>> m_prepareHooks;
    std::vector<Task> m_destroyHooks;
    std::mutex m_postedMutex;
    std::vector<Task> m_posted;
};

} // namespace cutefish::base
