// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "base/application_bundle.h"

#include <filesystem>

#include "base/file.h"
#include "base/paths.h"
#include "base/property_list.h"

namespace cutefish::base::bundle {

namespace fs = std::filesystem;

static constexpr std::string_view kSuffix = ".app";

std::string applicationsDirectory()
{
    return paths::home() + "/Applications";
}

std::string bundlePath(const std::string& path)
{
    if (path.empty())
        return {};
    std::error_code error;
    std::string result = fs::absolute(path, error).lexically_normal().string();
    while (result.size() > 1 && result.back() == '/')
        result.pop_back();
    const std::string name = fs::path(result).filename().string();
    if (name.size() <= kSuffix.size() || !name.ends_with(kSuffix))
        return {};
    return result;
}

std::string bundleOfProgram(const std::string& program)
{
    if (program.empty() || program.back() == '/')
        return {};
    std::error_code error;
    const fs::path macos = fs::absolute(program, error).lexically_normal().parent_path();
    const fs::path contents = macos.parent_path();
    if (macos.filename() != "MacOS" || contents.filename() != "Contents")
        return {};
    return bundlePath(contents.parent_path().string());
}

// As Exec quotes it: one argument, and a '%' that is no field code.
static std::string execArgument(const std::string& argument, bool literal)
{
    std::string result;
    for (const char c : argument) {
        if (literal && c == '%')
            result += '%';
        else if (c == ' ' || c == '\t' || c == '\n' || c == '"' || c == '\'' || c == '\\')
            result += '\\';
        result += c;
    }
    return result.empty() ? std::string("\"\"") : result;
}

// The handler's Exec with the bundle in its file or URL argument, or after
// the others when it takes none.
static std::string execFor(const std::string& path, const DesktopEntry& handler)
{
    // Field codes have to survive, so the handler's arguments are split with
    // every '%' escaped.
    DesktopEntry raw;
    raw.exec = handler.exec;
    for (size_t i = 0; i < raw.exec.size(); ++i) {
        if (raw.exec[i] == '%')
            raw.exec.replace(i++, 1, "%%");
    }

    std::string result;
    bool placed = false;
    for (const std::string& token : raw.command()) {
        if (!result.empty())
            result += ' ';
        if (!placed && (token == "%f" || token == "%F" || token == "%u" || token == "%U")) {
            result += execArgument(path, true);
            placed = true;
            continue;
        }
        result += execArgument(token, false);
    }
    if (!placed && !result.empty())
        result += ' ' + execArgument(path, true);
    return result;
}

std::optional<DesktopEntry> read(const std::string& path, const DesktopEntry& handler)
{
    const std::string bundle = bundlePath(path);
    if (bundle.empty())
        return std::nullopt;
    const std::optional<std::string> contents = file::read(bundle + "/Contents/Info.plist");
    if (!contents)
        return std::nullopt;
    const auto values = plist::rootStrings(*contents);
    if (!values)
        return std::nullopt;
    const auto value = [&values](const char* key) {
        const auto it = values->find(key);
        return it == values->end() ? std::string() : it->second;
    };

    DesktopEntry entry;
    entry.id = bundle;
    entry.path = bundle;
    entry.type = "Application";
    entry.bundle = true;
    entry.name = value("CFBundleDisplayName");
    if (entry.name.empty())
        entry.name = value("CFBundleName");
    if (entry.name.empty()) {
        entry.name = fs::path(bundle).filename().string();
        entry.name.resize(entry.name.size() - kSuffix.size());
    }

    std::string icon = value("CFBundleIconFile");
    if (!icon.empty() && icon.find('/') == std::string::npos) {
        if (fs::path(icon).extension().empty())
            icon += ".icns";
        icon = bundle + "/Contents/Resources/" + icon;
        std::error_code error;
        if (fs::is_regular_file(icon, error))
            entry.icon = icon;
    }

    entry.exec = execFor(bundle, handler);
    entry.tryExec = handler.tryExec;
    return entry;
}

} // namespace cutefish::base::bundle
