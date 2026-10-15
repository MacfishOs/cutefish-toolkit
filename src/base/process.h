// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <vector>

namespace cutefish::base {

class EventLoop;

// A program run in the background, as QProcess runs one: its output is
// collected and its end reported on the loop. The program is looked up on
// PATH; its environment is this process's.
class Process
{
public:
    explicit Process(EventLoop* loop = nullptr);
    // Kills a program still running and waits for it, as QProcess does.
    ~Process();

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    void setWorkingDirectory(std::string directory);
    // Written to the program's input, which is closed after it.
    void setStandardInput(std::string data);

    // False when the program cannot be started; onFinished follows otherwise.
    bool start(const std::string& program, const std::vector<std::string>& arguments = {});
    bool isRunning() const { return m_pid > 0; }
    pid_t pid() const { return m_pid; }

    // SIGTERM, SIGKILL, or any signal; false when the program is not running.
    bool terminate();
    bool kill();
    bool sendSignal(int signal);

    // Everything the program wrote so far.
    const std::string& standardOutput() const { return m_output; }
    const std::string& standardError() const { return m_error; }

    // As output arrives.
    std::function<void(std::string_view)> onStandardOutput;
    std::function<void(std::string_view)> onStandardError;
    // The exit code, or the signal that ended the program with crashed set.
    std::function<void(int exitCode, bool crashed)> onFinished;

    // Runs a program on its own, in a session of its own, as
    // QProcess::startDetached; its end is collected in the background. The
    // program's id, or -1.
    static pid_t startDetached(const std::string& program, const std::vector<std::string>& arguments = {},
                               const std::string& workingDirectory = {});

private:
    void readAvailable(int fd, std::string& into, const std::function<void(std::string_view)>& callback);
    void writeInput();
    void finish();
    void closeWatches();

    EventLoop& m_loop;
    std::string m_workingDirectory;
    std::string m_input;
    size_t m_inputWritten = 0;
    pid_t m_pid = -1;
    int m_pidfd = -1;
    int m_stdin = -1;
    int m_stdout = -1;
    int m_stderr = -1;
    int m_pidWatch = 0;
    int m_stdinWatch = 0;
    int m_stdoutWatch = 0;
    int m_stderrWatch = 0;
    std::string m_output;
    std::string m_error;
};

} // namespace cutefish::base
