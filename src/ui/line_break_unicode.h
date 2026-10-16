// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "include/core/SkRefCnt.h"

class SkUnicode;

namespace cutefish::ui {

// ICU for everything but soft line breaks, which come from line_breaks.h.
sk_sp<SkUnicode> makeLineBreakUnicode(sk_sp<SkUnicode> icu);

} // namespace cutefish::ui
