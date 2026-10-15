// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

#include "include/core/SkImage.h"
#include "include/core/SkRefCnt.h"

namespace cutefish::ui::thumbnails {

// The thumbnail of a file for a box of device pixels, from the freedesktop
// cache in ~/.cache/thumbnails or made and stored there, as cutefish-framework's
// ThumbnailCache does, so Qt programs and these share entries: the flavour is
// the first of 128, 256, 512 and 1024 that holds the box's longer side, and a
// file that cannot be read is marked in fail/cutefish. Null for such a file.
// On any thread; it decodes and writes files.
sk_sp<SkImage> request(const std::string& path, int width, int height);

// The cache file's URI key, as QUrl::fromLocalFile(path).toEncoded() writes it.
std::string fileUri(const std::string& absolutePath);

} // namespace cutefish::ui::thumbnails
