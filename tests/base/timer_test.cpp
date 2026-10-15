// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <chrono>
#include <memory>

#include "base/event_loop.h"
#include "base/timer.h"

using namespace cutefish::base;
using namespace std::chrono_literals;

TEST(TimerTest, CurrentLoopFollowsCreation)
{
    EventLoop outer;
    EXPECT_EQ(EventLoop::current(), &outer);
    {
        EventLoop inner;
        EXPECT_EQ(EventLoop::current(), &inner);
    }
    EXPECT_EQ(EventLoop::current(), &outer);
}

TEST(TimerTest, SingleShotFiresOnce)
{
    EventLoop loop;
    Timer timer;
    int count = 0;
    timer.setSingleShot(true);
    timer.onTimeout = [&] { ++count; };
    timer.start(2ms);
    EXPECT_TRUE(timer.isActive());
    loop.startTimer(30ms, false, [&] { loop.quit(); });
    loop.run();
    EXPECT_EQ(count, 1);
    EXPECT_FALSE(timer.isActive());
}

TEST(TimerTest, RestartPostponesAndStopCancels)
{
    EventLoop loop;
    Timer timer;
    timer.setSingleShot(true);
    int count = 0;
    timer.onTimeout = [&] { ++count; };
    timer.start(20ms);
    loop.startTimer(10ms, false, [&] { timer.start(); });
    loop.startTimer(25ms, false, [&] { EXPECT_EQ(count, 0); });
    loop.startTimer(45ms, false, [&] { loop.quit(); });
    loop.run();
    EXPECT_EQ(count, 1);

    timer.start(5ms);
    timer.stop();
    loop.startTimer(20ms, false, [&] { loop.quit(); });
    loop.run();
    EXPECT_EQ(count, 1);
}

TEST(TimerTest, RepeatingTimerMayDestroyItself)
{
    EventLoop loop;
    auto timer = std::make_unique<Timer>(&loop);
    int count = 0;
    timer->onTimeout = [&] {
        if (++count == 3)
            timer.reset();
    };
    timer->start(2ms);
    loop.startTimer(40ms, false, [&] { loop.quit(); });
    loop.run();
    EXPECT_EQ(count, 3);
}
