// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <optional>
#include <string>

#include "base/desktop_entry.h"

// Application bundles: folders named <name>.app that hold Contents/Info.plist
// and run as one program, Contents/MacOS/<CFBundleExecutable>. A desktop
// entry whose MimeType lists kType runs them.
namespace cutefish::base::bundle {

inline constexpr char kType[] = "application/x-apple-app";

// ~/Applications, where installed bundles go.
std::string applicationsDirectory();

// The path absolute and without a trailing slash when its name ends in .app;
// empty otherwise. Reads nothing.
std::string bundlePath(const std::string& path);
// The bundle whose Contents/MacOS the program is in, by the path alone;
// empty for any other program.
std::string bundleOfProgram(const std::string& program);

// The bundle at path as an application that handler starts with the path:
// named by CFBundleDisplayName, CFBundleName or the folder's own name, its
// icon Contents/Resources/<CFBundleIconFile>. Nothing when the folder holds
// no Info.plist that can be read.
std::optional<DesktopEntry> read(const std::string& path, const DesktopEntry& handler);

} // namespace cutefish::base::bundle
