// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui/color_mixer.h"

namespace cutefish::ui {

// The toolkit's recipes, which every provider starts from.
void addToolkitColors(ColorMixer& mixer, const Theme& theme);

} // namespace cutefish::ui
