// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace cutefish::base::url {

// "%41" and the like decoded; a malformed escape stays as it is.
std::string percentDecoded(std::string_view text);
// Every byte but unreserved ones (letters, digits, "-._~") and those in
// `keep` written as %XX.
std::string percentEncoded(std::string_view text, std::string_view keep = {});

// The path of a file: URL ("file:///a%20b" or "file://localhost/a"), or
// nothing for another scheme or host.
std::optional<std::string> toLocalFile(std::string_view url);
bool isLocalFile(std::string_view url);
// "file://" and the absolute path, with what needs it escaped.
std::string fromLocalFile(std::string_view path);

} // namespace cutefish::base::url
