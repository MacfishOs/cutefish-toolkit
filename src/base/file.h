// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace cutefish::base::file {

// The whole file; empty when it cannot be read.
std::optional<std::string> read(const std::string& path);

// Writes a temporary file next to the target and renames it over the
// target, so a reader sees the old or the new contents, never a part.
// Missing directories are created; an existing file's mode is kept.
bool writeAtomically(const std::string& path, std::string_view contents);

// A new directory, removed with everything in it when the object goes.
class TemporaryDirectory
{
public:
    // Under `parent` (the runtime directory's choice is the caller's), named
    // `prefix` and six random characters.
    explicit TemporaryDirectory(const std::string& parent, const std::string& prefix = "tmp");
    ~TemporaryDirectory();

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    bool isValid() const { return !m_path.empty(); }
    const std::string& path() const { return m_path; }
    // Keeps the directory when the object goes.
    void setAutoRemove(bool autoRemove) { m_autoRemove = autoRemove; }

private:
    std::string m_path;
    bool m_autoRemove = true;
};

// A new, empty file open for reading and writing, removed when the object
// goes.
class TemporaryFile
{
public:
    explicit TemporaryFile(const std::string& directory, const std::string& prefix = "tmp");
    ~TemporaryFile();

    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;

    bool isValid() const { return m_fd >= 0; }
    const std::string& path() const { return m_path; }
    // Close-on-exec; owned by the object.
    int fd() const { return m_fd; }
    bool write(std::string_view contents);

private:
    std::string m_path;
    int m_fd = -1;
};

} // namespace cutefish::base::file
