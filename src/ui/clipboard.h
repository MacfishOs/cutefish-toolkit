// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <string>

namespace cutefish::ui {

// The session's clipboard, for text. Copying makes this application its
// owner until another one copies.
class Clipboard
{
public:
    static void setText(const std::string& text);
    // Answers from the event loop, or at once in a headless application; the
    // text is empty when the clipboard holds none.
    static void requestText(std::function<void(std::string text)> callback);
};

} // namespace cutefish::ui
