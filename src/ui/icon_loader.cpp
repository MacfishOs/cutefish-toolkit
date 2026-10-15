// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/icon_loader.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <pwd.h>
#include <sstream>
#include <unistd.h>
#include <unordered_map>
#include <utility>

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
static const char* const kExtensions[] = {".png", ".svg"};

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
    const size_t start = text.find_first_not_of(" \t\r\n");
    const size_t end = text.find_last_not_of(" \t\r\n");
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

// The value of an attribute in a start tag's attribute text, and where it is.
static std::optional<std::pair<size_t, size_t>> attributeValue(const std::string& attributes, std::string_view name)
{
    for (size_t at = attributes.find(name); at != std::string::npos; at = attributes.find(name, at + 1)) {
        if (at > 0 && !std::isspace(static_cast<unsigned char>(attributes[at - 1])))
            continue;
        size_t i = at + name.size();
        while (i < attributes.size() && std::isspace(static_cast<unsigned char>(attributes[i])))
            ++i;
        if (i >= attributes.size() || attributes[i] != '=')
            continue;
        ++i;
        while (i < attributes.size() && std::isspace(static_cast<unsigned char>(attributes[i])))
            ++i;
        if (i >= attributes.size() || (attributes[i] != '"' && attributes[i] != '\''))
            continue;
        const size_t end = attributes.find(attributes[i], i + 1);
        if (end == std::string::npos)
            return std::nullopt;
        return std::pair{i + 1, end};
    }
    return std::nullopt;
}

// What Qt's SVG renderer reads and Skia's leaves out: <style> rules for
// class selectors, put into each element's style attribute ahead of its own
// declarations, and base64 data URIs broken over lines.
static std::string prepareSvg(std::string svg)
{
    for (size_t at = svg.find("data:"); at != std::string::npos; at = svg.find("data:", at + 5)) {
        const size_t end = svg.find_first_of("\"'", at);
        if (end == std::string::npos)
            break;
        std::string value = svg.substr(at, end - at);
        std::erase_if(value, [](char c) { return std::isspace(static_cast<unsigned char>(c)); });
        svg.replace(at, end - at, value);
    }

    // Skia lays a pattern out in user space whatever its units; one in the
    // bounding box units of the rect it fills gets that box as its transform.
    for (size_t at = svg.find("<pattern"); at != std::string::npos; at = svg.find("<pattern", at + 1)) {
        const size_t end = svg.find('>', at);
        if (end == std::string::npos)
            break;
        std::string tag = svg.substr(at, end - at);
        const auto value = [&tag](std::string_view name) {
            const auto found = attributeValue(tag, name);
            return found ? tag.substr(found->first, found->second - found->first) : std::string();
        };
        const std::string units = value("patternUnits");
        if (value("patternContentUnits") != "objectBoundingBox" || (!units.empty() && units != "objectBoundingBox")
            || !value("patternTransform").empty() || value("id").empty())
            continue;
        const size_t use = svg.find("url(#" + value("id") + ")");
        const size_t rect = use == std::string::npos ? std::string::npos : svg.rfind("<rect", use);
        if (rect == std::string::npos || svg.find('>', rect) < use)
            continue;
        const std::string rectTag = svg.substr(rect, svg.find('>', rect) - rect);
        const auto number = [&rectTag](std::string_view name, double fallback) {
            const auto found = attributeValue(rectTag, name);
            return found ? std::atof(rectTag.substr(found->first, found->second - found->first).c_str()) : fallback;
        };
        const double width = number("width", 0);
        const double height = number("height", 0);
        if (width <= 0 || height <= 0)
            continue;
        char transform[160];
        snprintf(transform, sizeof(transform), " patternTransform=\"matrix(%g 0 0 %g %g %g)\"", width, height,
                 number("x", 0), number("y", 0));
        svg.insert(end, transform);
    }

    std::unordered_map<std::string, std::string> rules;
    for (size_t at = svg.find("<style"); at != std::string::npos; at = svg.find("<style", at + 1)) {
        const size_t open = svg.find('>', at);
        const size_t close = svg.find("</style>", at);
        if (open == std::string::npos || close == std::string::npos)
            break;
        std::string css = svg.substr(open + 1, close - open - 1);
        for (const char* marker : {"<![CDATA[", "]]>"}) {
            if (const size_t found = css.find(marker); found != std::string::npos)
                css.erase(found, std::strlen(marker));
        }
        size_t from = 0;
        for (size_t brace = css.find('{'); brace != std::string::npos; brace = css.find('{', from)) {
            const size_t end = css.find('}', brace);
            if (end == std::string::npos)
                break;
            // Skia stops at an empty declaration, so they are joined without any.
            std::string declarations;
            std::stringstream parts(css.substr(brace + 1, end - brace - 1));
            std::string part;
            while (std::getline(parts, part, ';')) {
                part = trim(part);
                if (!part.empty())
                    declarations += part + ";";
            }
            std::stringstream selectors(css.substr(from, brace - from));
            std::string selector;
            while (std::getline(selectors, selector, ',')) {
                selector = trim(selector);
                const bool simpleClass = selector.size() > 1 && selector[0] == '.'
                        && selector.find_first_of(" .>+~:[#\t\n", 1) == std::string::npos;
                if (simpleClass)
                    rules[selector.substr(1)] += declarations;
            }
            from = end + 1;
        }
    }
    if (rules.empty())
        return svg;

    std::string out;
    out.reserve(svg.size());
    size_t from = 0;
    for (size_t tag = svg.find('<'); tag != std::string::npos; tag = svg.find('<', from)) {
        const size_t end = svg.find('>', tag);
        if (end == std::string::npos)
            break;
        std::string element = svg.substr(tag, end - tag);
        out.append(svg, from, tag - from);
        from = end;
        const auto classes = attributeValue(element, "class");
        if (element.size() < 2 || !std::isalpha(static_cast<unsigned char>(element[1])) || !classes) {
            out += element;
            continue;
        }
        std::string declarations;
        std::stringstream names(element.substr(classes->first, classes->second - classes->first));
        std::string name;
        while (names >> name) {
            if (const auto rule = rules.find(name); rule != rules.end())
                declarations += rule->second;
        }
        if (!declarations.empty()) {
            if (const auto style = attributeValue(element, "style"))
                element.insert(style->first, declarations);
            else
                element.insert(classes->second + 1, " style=\"" + declarations + "\"");
        }
        out += element;
    }
    out.append(svg, from, std::string::npos);
    return out;
}

static sk_sp<SkSVGDOM> loadSvg(sk_sp<SkData> data)
{
    const std::string text = prepareSvg(std::string(static_cast<const char*>(data->data()), data->size()));
    SkMemoryStream stream(text.data(), text.size(), false);
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

static sk_sp<SkImage> renderSvg(const sk_sp<SkSVGDOM>& svg, int size, const Color* tint, int height = 0)
{
    const int width = size;
    if (height <= 0)
        height = size;
    if (!svg || width <= 0)
        return nullptr;
    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
    if (!surface)
        return nullptr;
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    // The drawing fills the size it is drawn at, whatever size its file
    // declares, as Qt's SVG image reader draws it: the view box, or the
    // file's own size without one, fits centred, keeping its aspect whatever
    // the file asks.
    if (SkSVGSVG* root = svg->getRoot()) {
        const SkSVGLength fileWidth = root->getWidth();
        const SkSVGLength fileHeight = root->getHeight();
        const auto absolute = [](const SkSVGLength& length) {
            return length.value() > 0 && (length.unit() == SkSVGLength::Unit::kNumber
                                          || length.unit() == SkSVGLength::Unit::kPX);
        };
        if (!root->getViewBox().has_value() && absolute(fileWidth) && absolute(fileHeight))
            root->setViewBox(SkRect::MakeWH(fileWidth.value(), fileHeight.value()));
        root->setPreserveAspectRatio(SkSVGPreserveAspectRatio());
        root->setWidth(SkSVGLength(100, SkSVGLength::Unit::kPercentage));
        root->setHeight(SkSVGLength(100, SkSVGLength::Unit::kPercentage));
    }
    svg->setContainerSize(SkSize::Make(SkScalar(width), SkScalar(height)));
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

sk_sp<SkImage> IconLoader::glyph(const std::string& name, int size, Color color, double scale)
{
    // At 12 logical pixels and below the chevrons thin to under one; they
    // have heavier drawings for that, as FishUI's Icon picks them.
    std::string file = name;
    if (size / scale <= 12 + 1e-6 && name.starts_with("chevron-"))
        file += "-small";
    char suffix[48];
    snprintf(suffix, sizeof(suffix), "@%d#%08x", size, color.argb());
    const std::string key = "glyph:" + file + suffix;
    if (sk_sp<SkImage> image = cached(key))
        return image;

    sk_sp<SkData> data;
    if (const auto it = m_glyphs.find(name); it != m_glyphs.end()) {
        data = SkData::MakeWithoutCopy(it->second.data(), it->second.size());
    } else {
        const EmbeddedFile* embedded = findEmbeddedFile((file + ".svg").c_str());
        if (!embedded)
            embedded = findEmbeddedFile((name + ".svg").c_str());
        if (!embedded)
            return nullptr;
        data = SkData::MakeWithoutCopy(embedded->data, embedded->size);
    }
    sk_sp<SkImage> image = renderSvg(loadSvg(std::move(data)), size, &color);
    if (image)
        store(key, image);
    return image;
}

IconLoader::GtkCache::GtkCache(const std::string& base)
{
    namespace fs = std::filesystem;
    const fs::path file = base + "/icon-theme.cache";
    std::error_code error;
    const auto cacheTime = fs::last_write_time(file, error);
    if (error || cacheTime < fs::last_write_time(base, error) || error)
        return;
    std::ifstream stream(file, std::ios::binary);
    m_data.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    if (m_data.empty() || read16(0) != 1)
        return;
    m_valid = true;
    // Stale when any directory it lists changed after it.
    const uint32_t list = read32(8);
    const uint32_t count = read32(list);
    for (uint32_t i = 0; i < count && m_valid; ++i) {
        const uint32_t offset = read32(list + 4 + 4 * i);
        if (!m_valid || offset >= m_data.size()) {
            m_valid = false;
            break;
        }
        const auto directoryTime =
            fs::last_write_time(base + "/" + reinterpret_cast<const char*>(m_data.data() + offset), error);
        if (error || cacheTime < directoryTime)
            m_valid = false;
    }
}

uint16_t IconLoader::GtkCache::read16(size_t offset)
{
    if (m_data.size() < 2 || offset > m_data.size() - 2 || (offset & 1)) {
        m_valid = false;
        return 0;
    }
    return uint16_t(m_data[offset + 1] | m_data[offset] << 8);
}

uint32_t IconLoader::GtkCache::read32(size_t offset)
{
    if (m_data.size() < 4 || offset > m_data.size() - 4 || (offset & 3)) {
        m_valid = false;
        return 0;
    }
    return uint32_t(m_data[offset + 3]) | uint32_t(m_data[offset + 2]) << 8 | uint32_t(m_data[offset + 1]) << 16
            | uint32_t(m_data[offset]) << 24;
}

std::vector<std::string> IconLoader::GtkCache::lookup(const std::string& name)
{
    std::vector<std::string> directories;
    if (!m_valid || name.empty())
        return directories;
    // GTK's icon_name_hash, on signed chars.
    uint32_t hash = uint32_t(static_cast<signed char>(name[0]));
    for (size_t i = 1; i < name.size(); ++i)
        hash = (hash << 5) - hash + uint32_t(static_cast<signed char>(name[i]));
    const uint32_t table = read32(4);
    const uint32_t buckets = read32(table);
    if (!m_valid || buckets == 0) {
        m_valid = false;
        return directories;
    }
    for (uint32_t bucket = read32(table + 4 + (hash % buckets) * 4); bucket > 0 && bucket + 12 <= m_data.size();
         bucket = read32(bucket)) {
        const uint32_t nameOffset = read32(bucket + 4);
        if (nameOffset >= m_data.size() || name != reinterpret_cast<const char*>(m_data.data() + nameOffset))
            continue;
        const uint32_t list = read32(8);
        const uint32_t count = read32(list);
        const uint32_t images = read32(bucket + 8);
        const uint32_t length = read32(images);
        if (!m_valid || images + 4 + 8 * size_t(length) > m_data.size()) {
            m_valid = false;
            return {};
        }
        for (uint32_t j = 0; j < length && m_valid; ++j) {
            const uint32_t index = read16(images + 4 + 8 * j);
            const uint32_t offset = read32(list + 4 + index * 4);
            if (!m_valid || index >= count || offset >= m_data.size()) {
                m_valid = false;
                return {};
            }
            directories.emplace_back(reinterpret_cast<const char*>(m_data.data() + offset));
        }
        return directories;
    }
    return directories;
}

IconLoader::IconTheme& IconLoader::theme(const std::string& name)
{
    IconTheme& theme = m_themes[name];
    if (theme.loaded)
        return theme;
    theme.loaded = true;

    for (const std::string& base : iconBases()) {
        const std::string root = base + "/" + name;
        if (std::filesystem::is_directory(root)) {
            theme.bases.push_back(root);
            theme.caches.push_back(std::make_shared<GtkCache>(root));
        }
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
        directory.generic = entry["Context"] != "Applications" && entry["Context"] != "MimeTypes";
        theme.directories.push_back(directory);
    }
    return theme;
}

// directorySizeDistance in qiconloader.cpp, its Threshold quirk included.
static int sizeDistance(char type, int size, int minimum, int maximum, int threshold, int scale, int iconSize,
                        int iconScale)
{
    const int scaled = iconSize * iconScale;
    switch (type) {
    case 'F':
        return std::abs(size * scale - scaled);
    case 'S':
        if (scaled < minimum * scale)
            return minimum * scale - scaled;
        if (scaled > maximum * scale)
            return scaled - maximum * scale;
        return 0;
    default:
        if (scaled < (size - threshold) * scale)
            return minimum * scale - scaled;
        if (scaled > (size + threshold) * scale)
            return scaled - maximum * scale;
        return 0;
    }
}

// directoryMatchesSize in qiconloader.cpp.
static bool matchesSize(char type, int size, int minimum, int maximum, int threshold, int scale, int iconSize,
                        int iconScale)
{
    if (scale != iconScale)
        return false;
    switch (type) {
    case 'F':
        return size == iconSize;
    case 'S':
        return iconSize <= maximum && iconSize >= minimum;
    default:
        return iconSize >= size - threshold && iconSize <= size + threshold;
    }
}

// QIconLoader::findIconHelper and QIconLoaderEngine::entryForSize: the theme,
// its parents depth first, the first with the name giving every file it has
// of it, pictures before drawings; an exact size first, else the closest.
std::string IconLoader::lookup(const std::string& name, const std::string& requested, int size, int scale)
{
    struct Entry
    {
        const Directory* directory;
        std::string path;
    };
    const bool shortened = name.size() < requested.size();
    std::vector<std::string> visited;
    std::vector<Entry> entries;
    const auto search = [&](const auto& self, const std::string& themeName) -> void {
        if (themeName.empty() || std::find(visited.begin(), visited.end(), themeName) != visited.end())
            return;
        visited.push_back(themeName);
        IconTheme& iconTheme = theme(themeName);
        for (size_t i = 0; i < iconTheme.bases.size(); ++i) {
            const std::string& root = iconTheme.bases[i];
            // The cache's directories for the name, in its order, saves the stats.
            std::vector<const Directory*> directories;
            GtkCache& cache = *iconTheme.caches[i];
            const std::vector<std::string> cached = cache.lookup(name);
            if (cache.isValid()) {
                for (const std::string& path : cached) {
                    const auto found = std::find_if(iconTheme.directories.begin(), iconTheme.directories.end(),
                                                    [&path](const Directory& d) { return d.path == path; });
                    if (found != iconTheme.directories.end())
                        directories.push_back(&*found);
                }
            } else {
                for (const Directory& directory : iconTheme.directories)
                    directories.push_back(&directory);
            }
            for (const Directory* directory : directories) {
                if (shortened && !directory->generic)
                    continue;
                const std::string base = root + "/" + directory->path + "/" + name;
                if (std::filesystem::exists(base + ".png"))
                    entries.insert(entries.begin(), {directory, base + ".png"});
                else if (std::filesystem::exists(base + ".svg"))
                    entries.push_back({directory, base + ".svg"});
            }
        }
        if (!entries.empty())
            return;
        std::vector<std::string> parents = iconTheme.parents;
        if (themeName != "hicolor" && std::find(parents.begin(), parents.end(), "hicolor") == parents.end())
            parents.push_back("hicolor");
        for (const std::string& parent : parents) {
            self(self, parent);
            if (!entries.empty())
                return;
        }
    };
    search(search, Theme::current().iconTheme);

    for (const Entry& entry : entries) {
        const Directory& d = *entry.directory;
        if (matchesSize(d.type, d.size, d.minimum, d.maximum, d.threshold, d.scale, size, scale))
            return entry.path;
    }
    std::string best;
    int bestDistance = INT_MAX;
    for (const Entry& entry : entries) {
        const Directory& d = *entry.directory;
        const int distance = sizeDistance(d.type, d.size, d.minimum, d.maximum, d.threshold, d.scale, size, scale);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = entry.path;
        }
    }
    return best;
}

std::string IconLoader::themeIconPath(const std::string& name, int size, double scale)
{
    const int iconScale = int(std::ceil(scale));
    const std::string key = name + "@" + std::to_string(size) + "x" + std::to_string(iconScale) + "/"
            + Theme::current().iconTheme;
    const auto it = m_paths.find(key);
    if (it != m_paths.end())
        return it->second;

    // A missing "a-b-c" falls back to "a-b", then "a"; then /usr/share/pixmaps
    // is looked in for the name itself.
    std::string path;
    for (std::string candidate = name; !candidate.empty();) {
        path = lookup(candidate, name, size, iconScale);
        if (!path.empty())
            break;
        const size_t dash = candidate.rfind('-');
        candidate = dash == std::string::npos ? std::string() : candidate.substr(0, dash);
    }
    for (const char* extension : kExtensions) {
        if (!path.empty())
            break;
        if (std::filesystem::exists("/usr/share/pixmaps/" + name + extension))
            path = "/usr/share/pixmaps/" + name + extension;
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
    // A larger picture is scaled down smoothly, as QPixmap::scaled; a smaller
    // one stays as it is and the item stretches it, sampled linearly.
    const bool larger = decoded->width() > size || decoded->height() > size;
    const SkSamplingOptions sampling = larger ? SkSamplingOptions(SkCubicResampler::Mitchell())
                                              : SkSamplingOptions(SkFilterMode::kLinear);
    surface->getCanvas()->drawImageRect(decoded, SkRect::MakeWH(SkScalar(size), SkScalar(size)), sampling);
    return surface->makeImageSnapshot();
}

void IconLoader::registerGlyph(const std::string& name, std::string svg)
{
    m_glyphs[name] = std::move(svg);
    // Drawings of the name made before are stale.
    for (auto it = m_images.begin(); it != m_images.end();) {
        if (it->first.starts_with("glyph:" + name + "@")) {
            m_index.erase(it->first);
            it = m_images.erase(it);
        } else {
            ++it;
        }
    }
}

sk_sp<SkImage> IconLoader::renderSvgFile(const std::string& path, int width, int height)
{
    // The codecs for data URIs register with the loader.
    instance();
    sk_sp<SkData> data = SkData::MakeFromFileName(path.c_str());
    return data ? renderSvg(loadSvg(std::move(data)), width, nullptr, height) : nullptr;
}

sk_sp<SkImage> IconLoader::svg(const std::string& path, int width, int height, const Color* tint)
{
    char suffix[64];
    snprintf(suffix, sizeof(suffix), "@%dx%d#%08x%s", width, height, tint ? tint->argb() : 0, tint ? "t" : "");
    const std::string key = "svg:" + path + suffix;
    if (sk_sp<SkImage> image = cached(key))
        return image;
    sk_sp<SkData> data = SkData::MakeFromFileName(path.c_str());
    if (!data)
        return nullptr;
    sk_sp<SkImage> image = renderSvg(loadSvg(std::move(data)), width, tint, height);
    if (image)
        store(key, image);
    return image;
}

sk_sp<SkImage> IconLoader::themeIcon(const std::string& name, int size, int iconScale, int pixels)
{
    const std::string key = "theme:" + Theme::current().iconTheme + "/" + name + "@" + std::to_string(size) + "x"
            + std::to_string(iconScale) + ":" + std::to_string(pixels);
    if (sk_sp<SkImage> image = cached(key))
        return image;
    const std::string path = themeIconPath(name, size, iconScale);
    if (path.empty())
        return nullptr;
    sk_sp<SkImage> image = rasterise(path, pixels, nullptr);
    if (image)
        store(key, image);
    return image;
}

sk_sp<SkImage> IconLoader::themeIcon(const std::string& name, int size, double scale)
{
    const int device = int(std::lround(size * scale));
    const std::string key = "theme:" + Theme::current().iconTheme + "/" + name + "@" + std::to_string(size) + "x"
            + std::to_string(device);
    if (sk_sp<SkImage> image = cached(key))
        return image;
    const std::string path = themeIconPath(name, size, scale);
    if (path.empty())
        return nullptr;
    sk_sp<SkImage> image = rasterise(path, device, nullptr);
    if (image)
        store(key, image);
    return image;
}

} // namespace cutefish::ui
