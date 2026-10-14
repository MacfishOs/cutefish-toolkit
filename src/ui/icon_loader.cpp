// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/icon_loader.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <unistd.h>

#include "include/codec/SkCodec.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/codec/SkPngDecoder.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkData.h"
#include "include/core/SkPaint.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "modules/skresources/include/SkResources.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "modules/svg/include/SkSVGSVG.h"

#include "ui/embedded_files.h"
#include "ui/theme.h"

namespace cutefish::ui {

// Enough for every icon a few windows show at two scales.
static constexpr size_t kCacheLimit = 512;
static const char* const kExtensions[] = {".svg", ".png"};

static std::string homeDirectory()
{
    if (const passwd* entry = getpwuid(getuid()); entry && entry->pw_dir)
        return entry->pw_dir;
    return {};
}

// Where icon themes live, first match first.
static std::vector<std::string> iconBases()
{
    std::vector<std::string> bases;
    const std::string home = homeDirectory();
    if (!home.empty()) {
        bases.push_back(home + "/.icons");
        bases.push_back(home + "/.local/share/icons");
    }
    bases.push_back("/usr/local/share/icons");
    bases.push_back("/usr/share/icons");
    return bases;
}

static std::string trim(const std::string& text)
{
    const size_t start = text.find_first_not_of(" \t\r");
    const size_t end = text.find_last_not_of(" \t\r");
    return start == std::string::npos ? std::string() : text.substr(start, end - start + 1);
}

static std::vector<std::string> split(const std::string& text)
{
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, ',')) {
        part = trim(part);
        if (!part.empty())
            parts.push_back(part);
    }
    return parts;
}

static sk_sp<SkSVGDOM> loadSvg(sk_sp<SkData> data)
{
    SkMemoryStream stream(std::move(data));
    // Icons carry their raster parts, such as shadows, as data URIs.
    auto resources = skresources::DataURIResourceProviderProxy::Make(
        nullptr, skresources::ImageDecodeStrategy::kPreDecode);
    return SkSVGDOM::Builder().setResourceProvider(std::move(resources)).make(stream);
}

IconLoader& IconLoader::instance()
{
    static IconLoader loader = [] {
        // Codecs are opt-in: themes ship PNG files and PNG data inside SVG files.
        SkCodecs::Register(SkPngDecoder::Decoder());
        SkCodecs::Register(SkJpegDecoder::Decoder());
        return IconLoader();
    }();
    return loader;
}

sk_sp<SkImage> IconLoader::cached(const std::string& key)
{
    const auto it = m_index.find(key);
    if (it == m_index.end())
        return nullptr;
    m_images.splice(m_images.end(), m_images, it->second);
    return it->second->second;
}

void IconLoader::store(const std::string& key, sk_sp<SkImage> image)
{
    m_images.emplace_back(key, std::move(image));
    m_index[key] = std::prev(m_images.end());
    while (m_images.size() > kCacheLimit) {
        m_index.erase(m_images.front().first);
        m_images.pop_front();
    }
}

static sk_sp<SkImage> renderSvg(const sk_sp<SkSVGDOM>& svg, int size, const Color* tint)
{
    if (!svg || size <= 0)
        return nullptr;
    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(size, size));
    if (!surface)
        return nullptr;
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    // The drawing fills the size it is drawn at, whatever size its file declares.
    if (SkSVGSVG* root = svg->getRoot()) {
        root->setWidth(SkSVGLength(100, SkSVGLength::Unit::kPercentage));
        root->setHeight(SkSVGLength(100, SkSVGLength::Unit::kPercentage));
    }
    svg->setContainerSize(SkSize::Make(SkScalar(size), SkScalar(size)));
    if (tint) {
        // A single-colour glyph takes the colour wherever it has coverage.
        SkPaint paint;
        paint.setColorFilter(SkColorFilters::Blend(tint->argb(), SkBlendMode::kSrcIn));
        canvas->saveLayer(nullptr, &paint);
        svg->render(canvas);
        canvas->restore();
    } else {
        svg->render(canvas);
    }
    return surface->makeImageSnapshot();
}

sk_sp<SkImage> IconLoader::glyph(const std::string& name, int size, Color color)
{
    // Below 12 pixels the chevrons thin to under one; they have heavier drawings for that.
    std::string file = name;
    if (size <= 12 && name.starts_with("chevron-"))
        file += "-small";
    char suffix[48];
    snprintf(suffix, sizeof(suffix), "@%d#%08x", size, color.argb());
    const std::string key = "glyph:" + file + suffix;
    if (sk_sp<SkImage> image = cached(key))
        return image;

    const EmbeddedFile* embedded = findEmbeddedFile((file + ".svg").c_str());
    if (!embedded)
        embedded = findEmbeddedFile((name + ".svg").c_str());
    if (!embedded)
        return nullptr;
    sk_sp<SkImage> image = renderSvg(loadSvg(SkData::MakeWithoutCopy(embedded->data, embedded->size)),
                                     size, &color);
    if (image)
        store(key, image);
    return image;
}

IconLoader::IconTheme& IconLoader::theme(const std::string& name)
{
    IconTheme& theme = m_themes[name];
    if (theme.loaded)
        return theme;
    theme.loaded = true;

    for (const std::string& base : iconBases()) {
        const std::string root = base + "/" + name;
        if (std::filesystem::is_directory(root))
            theme.bases.push_back(root);
    }
    std::ifstream index;
    for (const std::string& root : theme.bases) {
        index.open(root + "/index.theme");
        if (index.is_open())
            break;
    }
    if (!index.is_open())
        return theme;
    theme.found = true;

    // Sections of index.theme, keyed by group name.
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> groups;
    std::string group;
    std::string line;
    while (std::getline(index, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        if (line.front() == '[' && line.back() == ']') {
            group = line.substr(1, line.size() - 2);
            continue;
        }
        const size_t equals = line.find('=');
        if (equals != std::string::npos)
            groups[group][trim(line.substr(0, equals))] = trim(line.substr(equals + 1));
    }

    auto& main = groups["Icon Theme"];
    theme.parents = split(main["Inherits"]);
    std::vector<std::string> directories = split(main["Directories"]);
    for (const std::string& scaled : split(main["ScaledDirectories"]))
        directories.push_back(scaled);
    for (const std::string& path : directories) {
        auto& entry = groups[path];
        Directory directory;
        directory.path = path;
        directory.size = std::atoi(entry["Size"].c_str());
        directory.scale = std::max(1, std::atoi(entry["Scale"].c_str()));
        const std::string& type = entry["Type"];
        directory.type = type == "Fixed" ? 'F' : type == "Scalable" ? 'S' : 'T';
        directory.minimum = entry.count("MinSize") ? std::atoi(entry["MinSize"].c_str()) : directory.size;
        directory.maximum = entry.count("MaxSize") ? std::atoi(entry["MaxSize"].c_str()) : directory.size;
        if (entry.count("Threshold"))
            directory.threshold = std::atoi(entry["Threshold"].c_str());
        theme.directories.push_back(directory);
    }
    return theme;
}

// How far a directory is from the wanted size, as the icon theme specification
// measures it; zero matches.
static int sizeDistance(int type, int size, int minimum, int maximum, int threshold, int wanted)
{
    switch (type) {
    case 'F':
        return std::abs(size - wanted);
    case 'S':
        if (wanted < minimum)
            return minimum - wanted;
        if (wanted > maximum)
            return wanted - maximum;
        return 0;
    default:
        if (wanted < size - threshold)
            return size - threshold - wanted;
        if (wanted > size + threshold)
            return wanted - size - threshold;
        return 0;
    }
}

std::string IconLoader::lookup(const std::string& name, int size)
{
    std::vector<std::string> order;
    std::vector<std::string> pending = {Theme::current().iconTheme};
    while (!pending.empty()) {
        const std::string next = pending.front();
        pending.erase(pending.begin());
        if (next.empty() || std::find(order.begin(), order.end(), next) != order.end())
            continue;
        order.push_back(next);
        for (const std::string& parent : theme(next).parents)
            pending.push_back(parent);
    }
    if (std::find(order.begin(), order.end(), "hicolor") == order.end())
        order.push_back("hicolor");

    for (const std::string& themeName : order) {
        IconTheme& iconTheme = theme(themeName);
        std::string best;
        int bestDistance = INT_MAX;
        for (const Directory& directory : iconTheme.directories) {
            const int distance = sizeDistance(directory.type, directory.size * directory.scale,
                                              directory.minimum * directory.scale,
                                              directory.maximum * directory.scale,
                                              directory.threshold * directory.scale, size);
            if (distance >= bestDistance)
                continue;
            for (const std::string& root : iconTheme.bases) {
                for (const char* extension : kExtensions) {
                    std::string path = root + "/" + directory.path + "/" + name + extension;
                    if (std::filesystem::exists(path)) {
                        best = std::move(path);
                        bestDistance = distance;
                        break;
                    }
                }
                if (bestDistance == distance)
                    break;
            }
            if (bestDistance == 0)
                break;
        }
        if (!best.empty())
            return best;
    }

    for (const char* extension : kExtensions) {
        std::string path = std::string("/usr/share/pixmaps/") + name + extension;
        if (std::filesystem::exists(path))
            return path;
    }
    return {};
}

std::string IconLoader::themeIconPath(const std::string& name, int size)
{
    const std::string key = name + "@" + std::to_string(size) + "/" + Theme::current().iconTheme;
    const auto it = m_paths.find(key);
    if (it != m_paths.end())
        return it->second;

    // A missing "a-b-c" falls back to "a-b", then "a".
    std::string path;
    for (std::string candidate = name; !candidate.empty();) {
        path = lookup(candidate, size);
        if (!path.empty())
            break;
        const size_t dash = candidate.rfind('-');
        candidate = dash == std::string::npos ? std::string() : candidate.substr(0, dash);
    }
    m_paths.emplace(key, path);
    return path;
}

sk_sp<SkImage> IconLoader::rasterise(const std::string& path, int size, const Color* tint)
{
    sk_sp<SkData> data = SkData::MakeFromFileName(path.c_str());
    if (!data)
        return nullptr;
    if (path.ends_with(".svg"))
        return renderSvg(loadSvg(std::move(data)), size, tint);

    std::unique_ptr<SkCodec> codec = SkCodec::MakeFromData(std::move(data));
    if (!codec)
        return nullptr;
    auto [decoded, result] = codec->getImage();
    if (!decoded)
        return nullptr;
    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(size, size));
    if (!surface)
        return nullptr;
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    surface->getCanvas()->drawImageRect(decoded, SkRect::MakeWH(SkScalar(size), SkScalar(size)),
                                        SkSamplingOptions(SkCubicResampler::Mitchell()));
    return surface->makeImageSnapshot();
}

sk_sp<SkImage> IconLoader::themeIcon(const std::string& name, int size)
{
    const std::string key = "theme:" + Theme::current().iconTheme + "/" + name + "@"
            + std::to_string(size);
    if (sk_sp<SkImage> image = cached(key))
        return image;
    const std::string path = themeIconPath(name, size);
    if (path.empty())
        return nullptr;
    sk_sp<SkImage> image = rasterise(path, size, nullptr);
    if (image)
        store(key, image);
    return image;
}

} // namespace cutefish::ui
