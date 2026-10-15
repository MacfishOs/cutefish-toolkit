// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "base/process.h"

#include <cerrno>
#include <csignal>
#include <memory>
#include <fcntl.h>
#include <spawn.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/event_loop.h"

extern char** environ;

namespace cutefish::base {

namespace {

struct SpawnSetup
{
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;

    SpawnSetup()
    {
        posix_spawn_file_actions_init(&actions);
        posix_spawnattr_init(&attributes);
        // The program starts with no signal blocked and default handlers, as
        // QProcess's child does, whatever this process changed.
        sigset_t none;
        sigemptyset(&none);
        posix_spawnattr_setsigmask(&attributes, &none);
        sigset_t defaults;
        sigfillset(&defaults);
        posix_spawnattr_setsigdefault(&attributes, &defaults);
    }
    ~SpawnSetup()
    {
        posix_spawn_file_actions_destroy(&actions);
        posix_spawnattr_destroy(&attributes);
    }
};

} // namespace

static std::vector<char*> argumentVector(const std::string& program, const std::vector<std::string>& arguments)
{
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(program.c_str()));
    for (const std::string& argument : arguments)
        argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);
    return argv;
}

// Through syscall(): glibc 2.41's sys/pidfd.h lacks the C linkage C++ needs.
static int openPidfd(pid_t pid)
{
    return int(syscall(SYS_pidfd_open, pid, 0));
}

static int signalPidfd(int pidfd, int signal)
{
    return int(syscall(SYS_pidfd_send_signal, pidfd, signal, nullptr, 0));
}

static void closeFd(int& fd)
{
    if (fd >= 0)
        ::close(fd);
    fd = -1;
}

// Collects a finished program's status from its pidfd; -1 while it runs.
static int reap(int pidfd, bool* crashed)
{
    siginfo_t info{};
    if (waitid(P_PIDFD, id_t(pidfd), &info, WEXITED | WNOHANG) != 0 || info.si_pid == 0)
        return -1;
    *crashed = info.si_code == CLD_KILLED || info.si_code == CLD_DUMPED;
    return info.si_status;
}

Process::Process(EventLoop* loop)
    : m_loop(loop ? *loop : *EventLoop::current())
{
}

Process::~Process()
{
    if (m_pid > 0) {
        ::kill(m_pid, SIGKILL);
        siginfo_t info{};
        waitid(P_PIDFD, id_t(m_pidfd), &info, WEXITED);
    }
    closeWatches();
}

void Process::setWorkingDirectory(std::string directory)
{
    m_workingDirectory = std::move(directory);
}

void Process::setStandardInput(std::string data)
{
    m_input = std::move(data);
}

bool Process::start(const std::string& program, const std::vector<std::string>& arguments)
{
    if (m_pid > 0)
        return false;
    closeWatches();
    m_output.clear();
    m_error.clear();
    m_inputWritten = 0;

    int input[2];
    int output[2];
    int error[2];
    if (pipe2(input, O_CLOEXEC) != 0)
        return false;
    if (pipe2(output, O_CLOEXEC) != 0) {
        ::close(input[0]);
        ::close(input[1]);
        return false;
    }
    if (pipe2(error, O_CLOEXEC) != 0) {
        ::close(input[0]);
        ::close(input[1]);
        ::close(output[0]);
        ::close(output[1]);
        return false;
    }

    SpawnSetup setup;
    posix_spawn_file_actions_adddup2(&setup.actions, input[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&setup.actions, output[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&setup.actions, error[1], STDERR_FILENO);
    if (!m_workingDirectory.empty())
        posix_spawn_file_actions_addchdir_np(&setup.actions, m_workingDirectory.c_str());
    posix_spawnattr_setflags(&setup.attributes, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
    std::vector<char*> argv = argumentVector(program, arguments);
    pid_t pid = -1;
    const int result = posix_spawnp(&pid, program.c_str(), &setup.actions, &setup.attributes, argv.data(), environ);
    ::close(input[0]);
    ::close(output[1]);
    ::close(error[1]);
    if (result != 0) {
        ::close(input[1]);
        ::close(output[0]);
        ::close(error[0]);
        return false;
    }

    m_pid = pid;
    m_pidfd = openPidfd(pid);
    m_stdin = input[1];
    m_stdout = output[0];
    m_stderr = error[0];
    for (int fd : {m_stdin, m_stdout, m_stderr})
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

    m_stdoutWatch = m_loop.watch(m_stdout, EPOLLIN, [this](uint32_t) {
        readAvailable(m_stdout, m_output, onStandardOutput);
    });
    m_stderrWatch = m_loop.watch(m_stderr, EPOLLIN, [this](uint32_t) {
        readAvailable(m_stderr, m_error, onStandardError);
    });
    if (m_input.empty())
        closeFd(m_stdin);
    else
        m_stdinWatch = m_loop.watch(m_stdin, EPOLLOUT, [this](uint32_t) { writeInput(); });
    m_pidWatch = m_loop.watch(m_pidfd, EPOLLIN, [this](uint32_t) { finish(); });
    return true;
}

void Process::readAvailable(int fd, std::string& into, const std::function<void(std::string_view)>& callback)
{
    char buffer[65536];
    while (true) {
        const ssize_t n = ::read(fd, buffer, sizeof(buffer));
        if (n > 0) {
            into.append(buffer, size_t(n));
            if (callback)
                callback(std::string_view(buffer, size_t(n)));
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        // At the end, the pipe stays readable; stop watching it.
        if (n == 0) {
            if (fd == m_stdout && m_stdoutWatch) {
                m_loop.unwatch(m_stdoutWatch);
                m_stdoutWatch = 0;
            } else if (fd == m_stderr && m_stderrWatch) {
                m_loop.unwatch(m_stderrWatch);
                m_stderrWatch = 0;
            }
        }
        return;
    }
}

void Process::writeInput()
{
    while (m_inputWritten < m_input.size()) {
        const ssize_t n = ::write(m_stdin, m_input.data() + m_inputWritten, m_input.size() - m_inputWritten);
        if (n > 0) {
            m_inputWritten += size_t(n);
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && errno == EAGAIN)
            return;
        break;
    }
    if (m_stdinWatch) {
        m_loop.unwatch(m_stdinWatch);
        m_stdinWatch = 0;
    }
    closeFd(m_stdin);
}

void Process::finish()
{
    bool crashed = false;
    const int status = reap(m_pidfd, &crashed);
    if (status < 0)
        return;
    // Output still in the pipes was written before the end.
    if (m_stdout >= 0)
        readAvailable(m_stdout, m_output, onStandardOutput);
    if (m_stderr >= 0)
        readAvailable(m_stderr, m_error, onStandardError);
    m_pid = -1;
    closeWatches();
    if (auto finished = onFinished)
        finished(status, crashed);
}

void Process::closeWatches()
{
    for (int* watch : {&m_pidWatch, &m_stdinWatch, &m_stdoutWatch, &m_stderrWatch}) {
        if (*watch)
            m_loop.unwatch(*watch);
        *watch = 0;
    }
    closeFd(m_pidfd);
    closeFd(m_stdin);
    closeFd(m_stdout);
    closeFd(m_stderr);
}

bool Process::sendSignal(int signal)
{
    return m_pid > 0 && signalPidfd(m_pidfd, signal) == 0;
}

bool Process::terminate()
{
    return sendSignal(SIGTERM);
}

bool Process::kill()
{
    return sendSignal(SIGKILL);
}

pid_t Process::startDetached(const std::string& program, const std::vector<std::string>& arguments,
                             const std::string& workingDirectory)
{
    SpawnSetup setup;
    if (!workingDirectory.empty())
        posix_spawn_file_actions_addchdir_np(&setup.actions, workingDirectory.c_str());
    posix_spawnattr_setflags(&setup.attributes, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSID);
    std::vector<char*> argv = argumentVector(program, arguments);
    pid_t pid = -1;
    if (posix_spawnp(&pid, program.c_str(), &setup.actions, &setup.attributes, argv.data(), environ) != 0)
        return -1;
    // Reaped when it ends, so it never lingers as a zombie; after this
    // process, init does it.
    const int pidfd = openPidfd(pid);
    EventLoop* loop = EventLoop::current();
    if (pidfd >= 0 && loop) {
        auto watch = std::make_shared<int>(0);
        *watch = loop->watch(pidfd, EPOLLIN, [loop, pidfd, watch](uint32_t) {
            bool crashed = false;
            if (reap(pidfd, &crashed) < 0)
                return;
            loop->unwatch(*watch);
            ::close(pidfd);
        });
    } else if (pidfd >= 0) {
        ::close(pidfd);
    }
    return pid;
}

} // namespace cutefish::base
