// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>
#include <vector>

// The shared-mime-info database as QMimeDatabase reads it, from the mime
// directories of ~/.local/share and the data directories: type names, their
// aliases and their parents. Read once, on first use; any thread.
namespace cutefish::base::mime {

// The type a name or alias stands for, such as "application/pdf" for
// "application/x-pdf"; empty for an unknown name.
std::string canonicalName(std::string_view name);
bool isKnown(std::string_view name);
// The other names of a type.
std::vector<std::string> aliases(std::string_view name);
// The types it is a subclass of, as QMimeType::parentMimeTypes() gives them:
// text types without one are text/plain's, other file types
// application/octet-stream's, unknown names included.
std::vector<std::string> parents(std::string_view name);

} // namespace cutefish::base::mime
