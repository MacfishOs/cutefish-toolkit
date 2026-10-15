// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <chrono>
#include <csignal>
#include <string>

#include "base/event_loop.h"
#include "base/process.h"
#include "base/timer.h"

using namespace cutefish::base;
using namespace std::chrono_literals;

namespace {

// Runs the loop until the process ends or five seconds pass.
bool runUntilFinished(EventLoop& loop, Process& process, int* exitCode = nullptr, bool* crashed = nullptr)
{
    bool finished = false;
    process.onFinished = [&](int code, bool killed) {
        finished = true;
        if (exitCode)
            *exitCode = code;
        if (crashed)
            *crashed = killed;
        loop.quit();
    };
    Timer timeout(&loop);
    timeout.setSingleShot(true);
    timeout.onTimeout = [&loop] { loop.quit(); };
    timeout.start(5s);
    loop.run();
    return finished;
}

} // namespace

TEST(ProcessTest, CollectsOutputAndExitCode)
{
    EventLoop loop;
    Process process(&loop);
    ASSERT_TRUE(process.start("sh", {"-c", "printf 'out'; printf 'err' >&2; exit 3"}));
    EXPECT_TRUE(process.isRunning());
    int exitCode = -1;
    bool crashed = true;
    ASSERT_TRUE(runUntilFinished(loop, process, &exitCode, &crashed));
    EXPECT_EQ(exitCode, 3);
    EXPECT_FALSE(crashed);
    EXPECT_EQ(process.standardOutput(), "out");
    EXPECT_EQ(process.standardError(), "err");
    EXPECT_FALSE(process.isRunning());
}

TEST(ProcessTest, WritesInputAndRunsInTheWorkingDirectory)
{
    EventLoop loop;
    Process process(&loop);
    process.setStandardInput(std::string(200000, 'x'));
    process.setWorkingDirectory("/tmp");
    ASSERT_TRUE(process.start("sh", {"-c", "wc -c; pwd"}));
    ASSERT_TRUE(runUntilFinished(loop, process));
    EXPECT_EQ(process.standardOutput(), "200000\n/tmp\n");
}

TEST(ProcessTest, ReportsSignals)
{
    EventLoop loop;
    Process process(&loop);
    ASSERT_TRUE(process.start("sleep", {"30"}));
    EXPECT_TRUE(process.terminate());
    int signal = 0;
    bool crashed = false;
    ASSERT_TRUE(runUntilFinished(loop, process, &signal, &crashed));
    EXPECT_TRUE(crashed);
    EXPECT_EQ(signal, SIGTERM);
}

TEST(ProcessTest, RefusesMissingPrograms)
{
    EventLoop loop;
    Process process(&loop);
    EXPECT_FALSE(process.start("cutefish-no-such-program"));
    EXPECT_FALSE(process.isRunning());
}

TEST(ProcessTest, StartsDetachedInASessionOfItsOwn)
{
    EventLoop loop;
    const pid_t pid = Process::startDetached("sleep", {"0.1"});
    ASSERT_GT(pid, 0);
    EXPECT_EQ(getsid(pid), pid);
    // Reaped by the loop once it ends.
    Timer wait(&loop);
    wait.setSingleShot(true);
    wait.onTimeout = [&loop] { loop.quit(); };
    wait.start(500ms);
    loop.run();
    EXPECT_NE(kill(pid, 0), 0);
}
