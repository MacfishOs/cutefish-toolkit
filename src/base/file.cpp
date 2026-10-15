// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "base/file.h"

#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace cutefish::base::file {

std::optional<std::string> read(const std::string& path)
{
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return std::nullopt;

    std::string contents;
    char buffer[16384];
    while (true) {
        const ssize_t size = ::read(fd, buffer, sizeof(buffer));
        if (size < 0 && errno == EINTR)
            continue;
        if (size < 0) {
            close(fd);
            return std::nullopt;
        }
        if (size == 0)
            break;
        contents.append(buffer, size_t(size));
    }
    close(fd);
    return contents;
}

// umask() can only be read by changing it, which races with other threads.
static mode_t currentUmask()
{
    const std::optional<std::string> status = read("/proc/self/status");
    const size_t start = status ? status->find("Umask:\t") : std::string::npos;
    if (start == std::string::npos)
        return 022;
    return mode_t(std::stoul(status->substr(start + 7, 4), nullptr, 8));
}

static bool writeAll(int fd, std::string_view contents)
{
    while (!contents.empty()) {
        const ssize_t written = ::write(fd, contents.data(), contents.size());
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return false;
        contents.remove_prefix(size_t(written));
    }
    return true;
}

bool writeAtomically(const std::string& path, std::string_view contents)
{
    const std::filesystem::path target(path);
    std::error_code error;
    if (target.has_parent_path())
        std::filesystem::create_directories(target.parent_path(), error);

    std::string temporary = path + ".XXXXXX";
    const int fd = mkostemp(temporary.data(), O_CLOEXEC);
    if (fd < 0)
        return false;

    struct stat existing{};
    if (stat(path.c_str(), &existing) == 0)
        fchmod(fd, existing.st_mode & 07777);
    else
        fchmod(fd, 0666 & ~currentUmask());

    const bool written = writeAll(fd, contents) && fdatasync(fd) == 0;
    close(fd);
    if (!written || rename(temporary.c_str(), path.c_str()) != 0) {
        unlink(temporary.c_str());
        return false;
    }
    return true;
}

} // namespace cutefish::base::file
