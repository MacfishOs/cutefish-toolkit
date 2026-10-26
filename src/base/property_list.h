// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

// Property lists, the XML and binary (bplist00) files application bundles
// describe themselves in.
namespace cutefish::base::plist {

// The string values of the root dictionary, as UTF-8; values of other types
// are left out. Nothing when the data is neither form or its root is not a
// dictionary.
std::optional<std::map<std::string, std::string>> rootStrings(std::string_view data);

} // namespace cutefish::base::plist
