// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace cutefish::ui {

// QCharAttributes as QTextEngine computes them, one per UTF-16 position, the
// end included: whether UAX #14, as Qt 6.8 tailors it, allows a break
// before the position, and whether the code unit there is white space Qt
// wraps at.
struct QtCharAttributes
{
    std::vector<uint8_t> lineBreak;
    std::vector<uint8_t> breakableSpace;
};

QtCharAttributes qtCharAttributes(std::u16string_view text);

// Where Qt's text layout may wrap a line: QUnicodeTools::getLineBreaks with
// QTextEngine's options, Qt 6.8's pair table tailoring of UAX #14 rather
// than ICU's rules, and after any breakable white space, as QTextLine does.
// One entry per UTF-16 position, the end included; true allows a break
// before it.
std::vector<uint8_t> qtLineBreaks(std::u16string_view text);

} // namespace cutefish::ui
