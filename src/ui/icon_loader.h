// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "include/core/SkImage.h"
#include "include/core/SkRefCnt.h"

#include "ui/geometry.h"

namespace cutefish::ui {

// Icons rasterised at the exact device size they are drawn at, so they land on
// whole pixels at every scale, and kept for reuse.
class IconLoader
{
public:
    static IconLoader& instance();

    // One of the toolkit's single-colour glyphs, in the colour.
    sk_sp<SkImage> glyph(const std::string& name, int size, Color color);
    // An icon of the theme, its parents or hicolor; null when none has it.
    sk_sp<SkImage> themeIcon(const std::string& name, int size);
    // The file the theme lookup settles on, for tests.
    std::string themeIconPath(const std::string& name, int size);

private:
    struct Directory
    {
        std::string path;
        int size = 0;
        int minimum = 0;
        int maximum = 0;
        int threshold = 2;
        int scale = 1;
        // Fixed, Scalable or Threshold.
        char type = 'T';
    };

    struct IconTheme
    {
        std::vector<std::string> bases;
        std::vector<Directory> directories;
        std::vector<std::string> parents;
        bool loaded = false;
        bool found = false;
    };

    IconTheme& theme(const std::string& name);
    std::string lookup(const std::string& name, int size);
    sk_sp<SkImage> rasterise(const std::string& path, int size, const Color* tint);
    sk_sp<SkImage> cached(const std::string& key);
    void store(const std::string& key, sk_sp<SkImage> image);

    std::unordered_map<std::string, IconTheme> m_themes;
    std::unordered_map<std::string, std::string> m_paths;
    // Least recently used last.
    std::list<std::pair<std::string, sk_sp<SkImage>>> m_images;
    std::unordered_map<std::string, std::list<std::pair<std::string, sk_sp<SkImage>>>::iterator> m_index;
};

} // namespace cutefish::ui
