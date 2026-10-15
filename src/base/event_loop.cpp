// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "base/event_loop.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

namespace cutefish::base {

static thread_local EventLoop* s_current = nullptr;

bool LoopHandle::post(std::function<void()> task) const
{
    if (!m_link)
        return false;
    std::lock_guard lock(m_link->mutex);
    if (!m_link->loop)
        return false;
    m_link->loop->post(std::move(task));
    return true;
}

EventLoop::EventLoop()
    : m_previous(s_current)
{
    s_current = this;
    m_handle.m_link = std::make_shared<LoopHandle::Link>();
    m_handle.m_link->loop = this;
    m_epoll = epoll_create1(EPOLL_CLOEXEC);
    m_wakeFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    watch(m_wakeFd, EPOLLIN, [this](uint32_t) {
        uint64_t count;
        while (read(m_wakeFd, &count, sizeof(count)) > 0) {
        }
    });
}

EventLoop::~EventLoop()
{
    // A hook may add hooks.
    while (!m_destroyHooks.empty()) {
        Task hook = std::move(m_destroyHooks.back());
        m_destroyHooks.pop_back();
        hook();
    }
    // A dropped task may own views whose destructors still unwatch here.
    dropPosted();
    {
        std::lock_guard lock(m_handle.m_link->mutex);
        m_handle.m_link->loop = nullptr;
    }
    if (s_current == this)
        s_current = m_previous;
    for (const auto& [id, watch] : m_watches) {
        if (watch->ownsFd)
            close(watch->fd);
    }
    close(m_wakeFd);
    close(m_epoll);
}

int EventLoop::watch(int fd, uint32_t events, FdCallback callback)
{
    const int id = m_nextId++;
    epoll_event event{};
    event.events = events;
    event.data.u64 = uint64_t(id);
    if (epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &event) < 0) {
        perror("epoll_ctl");
        return 0;
    }

    auto watch = std::make_shared<Watch>();
    watch->fd = fd;
    watch->callback = std::move(callback);
    m_watches.emplace(id, std::move(watch));
    return id;
}

EventLoop* EventLoop::current()
{
    return s_current;
}

LoopHandle EventLoop::handle() const
{
    return m_handle;
}

void EventLoop::setWatchEvents(int id, uint32_t events)
{
    const auto it = m_watches.find(id);
    if (it == m_watches.end())
        return;

    epoll_event event{};
    event.events = events;
    event.data.u64 = uint64_t(id);
    epoll_ctl(m_epoll, EPOLL_CTL_MOD, it->second->fd, &event);
}

void EventLoop::unwatch(int id)
{
    const auto it = m_watches.find(id);
    if (it == m_watches.end())
        return;

    epoll_ctl(m_epoll, EPOLL_CTL_DEL, it->second->fd, nullptr);
    if (it->second->ownsFd)
        close(it->second->fd);
    m_watches.erase(it);
}

int EventLoop::startTimer(std::chrono::nanoseconds interval, bool repeat, Task task)
{
    const int fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (fd < 0) {
        perror("timerfd_create");
        return 0;
    }

    // A zero expiry would disarm the timer.
    const int64_t nanoseconds = std::max<int64_t>(interval.count(), 1);
    itimerspec spec{};
    spec.it_value.tv_sec = time_t(nanoseconds / 1000000000);
    spec.it_value.tv_nsec = long(nanoseconds % 1000000000);
    if (repeat)
        spec.it_interval = spec.it_value;
    timerfd_settime(fd, 0, &spec, nullptr);

    const int id = watch(fd, EPOLLIN, nullptr);
    if (!id) {
        close(fd);
        return 0;
    }
    auto& timer = m_watches.at(id);
    timer->ownsFd = true;
    timer->callback = [this, id, fd, repeat, task = std::move(task)](uint32_t) {
        uint64_t expirations;
        if (read(fd, &expirations, sizeof(expirations)) <= 0)
            return;
        if (!repeat)
            unwatch(id);
        task();
    };
    return id;
}

void EventLoop::stopTimer(int id)
{
    unwatch(id);
}

void EventLoop::post(Task task)
{
    {
        std::lock_guard lock(m_postedMutex);
        m_posted.push_back(std::move(task));
    }
    const uint64_t one = 1;
    [[maybe_unused]] const auto written = write(m_wakeFd, &one, sizeof(one));
}

void EventLoop::dropPosted()
{
    // Destroying a task may post another.
    while (true) {
        std::vector<Task> posted;
        {
            std::lock_guard lock(m_postedMutex);
            posted.swap(m_posted);
        }
        if (posted.empty())
            break;
    }
}

void EventLoop::addDestroyHook(Task hook)
{
    m_destroyHooks.push_back(std::move(hook));
}

int EventLoop::addPrepareHook(Task hook)
{
    const int id = m_nextId++;
    m_prepareHooks.emplace_back(id, std::move(hook));
    return id;
}

void EventLoop::removePrepareHook(int id)
{
    std::erase_if(m_prepareHooks, [id](const auto& hook) { return hook.first == id; });
}

int EventLoop::run()
{
    m_running = true;
    epoll_event events[32];

    while (m_running) {
        runPosted();
        // A hook may remove hooks.
        const auto hooks = m_prepareHooks;
        for (const auto& [id, hook] : hooks)
            hook();
        if (!m_running)
            break;

        const int count = epoll_wait(m_epoll, events, 32, hasPosted() ? 0 : -1);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            perror("epoll_wait");
            return 1;
        }

        for (int i = 0; i < count; ++i) {
            const auto it = m_watches.find(int(events[i].data.u64));
            if (it == m_watches.end())
                continue;
            const auto watch = it->second;
            if (watch->callback)
                watch->callback(events[i].events);
        }
    }
    return m_exitCode;
}

void EventLoop::quit(int exitCode)
{
    m_exitCode = exitCode;
    m_running = false;
}

void EventLoop::runPosted()
{
    std::vector<Task> tasks;
    {
        std::lock_guard lock(m_postedMutex);
        tasks.swap(m_posted);
    }
    for (const auto& task : tasks)
        task();
}

bool EventLoop::hasPosted()
{
    std::lock_guard lock(m_postedMutex);
    return !m_posted.empty();
}

} // namespace cutefish::base
