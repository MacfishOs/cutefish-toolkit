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

} // namespace cutefish::base::file
