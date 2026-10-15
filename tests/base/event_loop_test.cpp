// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "base/event_loop.h"

using namespace cutefish::base;
using namespace std::chrono_literals;

TEST(EventLoopTest, PostFromAnotherThreadWakesTheLoop)
{
    EventLoop loop;
    std::thread thread([&loop] { loop.post([&loop] { loop.quit(7); }); });
    EXPECT_EQ(loop.run(), 7);
    thread.join();
}

TEST(EventLoopTest, TimersFireInOrder)
{
    EventLoop loop;
    std::vector<int> fired;
    loop.startTimer(20ms, false, [&] { fired.push_back(2); });
    loop.startTimer(5ms, false, [&] { fired.push_back(1); });
    loop.startTimer(40ms, false, [&] { loop.quit(); });
    loop.run();
    EXPECT_EQ(fired, (std::vector<int>{1, 2}));
}

TEST(EventLoopTest, RepeatingTimerStopsFromItsCallback)
{
    EventLoop loop;
    int count = 0;
    int timer = 0;
    timer = loop.startTimer(2ms, true, [&] {
        if (++count == 3)
            loop.stopTimer(timer);
    });
    loop.startTimer(50ms, false, [&] { loop.quit(); });
    loop.run();
    EXPECT_EQ(count, 3);
}

TEST(EventLoopTest, PostedTasksMayPostMore)
{
    EventLoop loop;
    int depth = 0;
    std::function<void()> task = [&] {
        if (++depth < 5)
            loop.post(task);
        else
            loop.quit();
    };
    loop.post(task);
    loop.run();
    EXPECT_EQ(depth, 5);
}

TEST(EventLoopTest, DroppingPostedTasksMayStopTimers)
{
    auto loop = std::make_unique<EventLoop>();
    const int timer = loop->startTimer(1s, false, [] {});
    EventLoop* raw = loop.get();
    // Owned by the task, as a window a closing dialog posts for deletion.
    auto stopper = std::shared_ptr<int>(new int(0), [raw, timer](int* value) {
        raw->stopTimer(timer);
        delete value;
    });
    loop->post([stopper] {});
    stopper.reset();
    loop.reset();
}
