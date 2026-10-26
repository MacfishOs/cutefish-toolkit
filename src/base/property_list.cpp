// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "base/property_list.h"

#include <cstdint>
#include <cstdlib>

namespace cutefish::base::plist {

using Strings = std::map<std::string, std::string>;

static void appendUtf8(std::string* out, uint32_t code)
{
    if (code < 0x80) {
        *out += char(code);
    } else if (code < 0x800) {
        *out += char(0xc0 | (code >> 6));
        *out += char(0x80 | (code & 0x3f));
    } else if (code < 0x10000) {
        *out += char(0xe0 | (code >> 12));
        *out += char(0x80 | ((code >> 6) & 0x3f));
        *out += char(0x80 | (code & 0x3f));
    } else {
        *out += char(0xf0 | (code >> 18));
        *out += char(0x80 | ((code >> 12) & 0x3f));
        *out += char(0x80 | ((code >> 6) & 0x3f));
        *out += char(0x80 | (code & 0x3f));
    }
}

class XmlReader
{
public:
    explicit XmlReader(std::string_view text)
        : m_text(text)
    {
    }

    struct Tag
    {
        std::string name;
        bool closing = false;
        bool empty = false;
    };

    // The next element tag, past text, comments, declarations and
    // processing instructions; nothing at the end or on malformed markup.
    std::optional<Tag> nextTag()
    {
        while (true) {
            const size_t open = m_text.find('<', m_position);
            if (open == std::string_view::npos)
                return std::nullopt;
            m_position = open;
            if (skipped("<!--", "-->") || skipped("<?", "?>") || skipped("<![CDATA[", "]]>"))
                continue;
            if (m_text.substr(m_position).starts_with("<!")) {
                if (!skipDeclaration())
                    return std::nullopt;
                continue;
            }
            const size_t close = m_text.find('>', m_position);
            if (close == std::string_view::npos)
                return std::nullopt;
            std::string_view inside = m_text.substr(m_position + 1, close - m_position - 1);
            m_position = close + 1;

            Tag tag;
            if (inside.starts_with('/')) {
                tag.closing = true;
                inside.remove_prefix(1);
            }
            if (inside.ends_with('/')) {
                tag.empty = true;
                inside.remove_suffix(1);
            }
            const size_t end = inside.find_first_of(" \t\r\n");
            tag.name = std::string(inside.substr(0, end));
            if (tag.name.empty())
                return std::nullopt;
            return tag;
        }
    }

    // The text up to the closing tag of the element just opened.
    std::optional<std::string> text(const std::string& name)
    {
        const std::string closing = "</" + name;
        std::string result;
        while (true) {
            const size_t next = m_text.find('<', m_position);
            if (next == std::string_view::npos)
                return std::nullopt;
            if (!decode(m_text.substr(m_position, next - m_position), &result))
                return std::nullopt;
            m_position = next;
            if (m_text.substr(m_position).starts_with("<![CDATA[")) {
                const size_t end = m_text.find("]]>", m_position);
                if (end == std::string_view::npos)
                    return std::nullopt;
                result += m_text.substr(m_position + 9, end - m_position - 9);
                m_position = end + 3;
                continue;
            }
            if (skipped("<!--", "-->"))
                continue;
            if (!m_text.substr(m_position).starts_with(closing))
                return std::nullopt;
            const std::optional<Tag> tag = nextTag();
            if (!tag || !tag->closing || tag->name != name)
                return std::nullopt;
            return result;
        }
    }

    // Past the end of the element just opened.
    bool skipElement(const Tag& opened)
    {
        if (opened.empty)
            return true;
        int depth = 1;
        while (depth > 0) {
            const std::optional<Tag> tag = nextTag();
            if (!tag)
                return false;
            if (tag->closing)
                --depth;
            else if (!tag->empty)
                ++depth;
        }
        return true;
    }

private:
    bool skipped(std::string_view start, std::string_view end)
    {
        if (!m_text.substr(m_position).starts_with(start))
            return false;
        const size_t found = m_text.find(end, m_position + start.size());
        m_position = found == std::string_view::npos ? m_text.size() : found + end.size();
        return true;
    }

    // <!DOCTYPE ...>, whose internal subset in brackets may hold '>'.
    bool skipDeclaration()
    {
        int brackets = 0;
        for (size_t i = m_position + 2; i < m_text.size(); ++i) {
            if (m_text[i] == '[') {
                ++brackets;
            } else if (m_text[i] == ']') {
                --brackets;
            } else if (m_text[i] == '>' && brackets <= 0) {
                m_position = i + 1;
                return true;
            }
        }
        return false;
    }

    static bool decode(std::string_view text, std::string* out)
    {
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] != '&') {
                *out += text[i];
                continue;
            }
            const size_t end = text.find(';', i);
            if (end == std::string_view::npos)
                return false;
            const std::string_view entity = text.substr(i + 1, end - i - 1);
            if (entity == "amp") {
                *out += '&';
            } else if (entity == "lt") {
                *out += '<';
            } else if (entity == "gt") {
                *out += '>';
            } else if (entity == "quot") {
                *out += '"';
            } else if (entity == "apos") {
                *out += '\'';
            } else if (entity.starts_with('#') && entity.size() > 1) {
                const bool hex = entity[1] == 'x' || entity[1] == 'X';
                const std::string digits(entity.substr(hex ? 2 : 1));
                char* parsed = nullptr;
                const unsigned long code = std::strtoul(digits.c_str(), &parsed, hex ? 16 : 10);
                if (digits.empty() || *parsed || code == 0 || code > 0x10ffff || (code >= 0xd800 && code < 0xe000))
                    return false;
                appendUtf8(out, uint32_t(code));
            } else {
                return false;
            }
            i = end;
        }
        return true;
    }

    std::string_view m_text;
    size_t m_position = 0;
};

static std::optional<Strings> readXml(std::string_view data)
{
    XmlReader reader(data);
    std::optional<XmlReader::Tag> root = reader.nextTag();
    if (root && root->name == "plist" && !root->closing && !root->empty)
        root = reader.nextTag();
    if (!root || root->name != "dict" || root->closing)
        return std::nullopt;

    Strings result;
    if (root->empty)
        return result;
    while (true) {
        const std::optional<XmlReader::Tag> key = reader.nextTag();
        if (!key)
            return std::nullopt;
        if (key->closing && key->name == "dict")
            return result;
        if (key->closing || key->name != "key")
            return std::nullopt;
        std::optional<std::string> name = key->empty ? std::string() : reader.text("key");
        if (!name)
            return std::nullopt;

        const std::optional<XmlReader::Tag> value = reader.nextTag();
        if (!value || value->closing)
            return std::nullopt;
        if (value->name == "string") {
            std::optional<std::string> text = value->empty ? std::string() : reader.text("string");
            if (!text)
                return std::nullopt;
            result.emplace(std::move(*name), std::move(*text));
        } else if (!reader.skipElement(*value)) {
            return std::nullopt;
        }
    }
}

class BinaryReader
{
public:
    explicit BinaryReader(std::string_view data)
        : m_data(data)
    {
    }

    std::optional<Strings> read()
    {
        if (m_data.size() < 8 + 32)
            return std::nullopt;
        const size_t trailer = m_data.size() - 32;
        m_offsetSize = uint8_t(m_data[trailer + 6]);
        m_referenceSize = uint8_t(m_data[trailer + 7]);
        m_objectCount = number(trailer + 8, 8);
        const uint64_t top = number(trailer + 16, 8);
        m_offsetTable = number(trailer + 24, 8);
        if (m_offsetSize < 1 || m_offsetSize > 8 || m_referenceSize < 1 || m_referenceSize > 8
            || m_offsetTable >= trailer || m_objectCount > (trailer - m_offsetTable) / m_offsetSize)
            return std::nullopt;

        const std::optional<size_t> root = objectOffset(top);
        if (!root || (uint8_t(m_data[*root]) >> 4) != 0xd)
            return std::nullopt;
        size_t start = 0;
        const std::optional<uint64_t> count = length(*root, &start);
        if (!count || *count > (m_data.size() - start) / (2 * m_referenceSize))
            return std::nullopt;

        Strings result;
        for (uint64_t i = 0; i < *count; ++i) {
            const uint64_t keyReference = number(start + i * m_referenceSize, m_referenceSize);
            const uint64_t valueReference = number(start + (*count + i) * m_referenceSize, m_referenceSize);
            std::optional<std::string> key = string(keyReference);
            if (!key)
                return std::nullopt;
            if (std::optional<std::string> value = string(valueReference))
                result.emplace(std::move(*key), std::move(*value));
        }
        return result;
    }

private:
    uint64_t number(size_t position, size_t size) const
    {
        uint64_t value = 0;
        for (size_t i = 0; i < size; ++i)
            value = value << 8 | uint8_t(m_data[position + i]);
        return value;
    }

    std::optional<size_t> objectOffset(uint64_t object) const
    {
        if (object >= m_objectCount)
            return std::nullopt;
        const uint64_t offset = number(size_t(m_offsetTable + object * m_offsetSize), m_offsetSize);
        if (offset < 8 || offset >= m_offsetTable)
            return std::nullopt;
        return size_t(offset);
    }

    // The element count in an object's marker, or in the integer after it;
    // start is where the elements begin.
    std::optional<uint64_t> length(size_t offset, size_t* start) const
    {
        const uint8_t info = uint8_t(m_data[offset]) & 0xf;
        if (info != 0xf) {
            *start = offset + 1;
            return info;
        }
        if (offset + 2 > m_offsetTable)
            return std::nullopt;
        const uint8_t marker = uint8_t(m_data[offset + 1]);
        if ((marker >> 4) != 0x1 || (marker & 0xf) > 3)
            return std::nullopt;
        const size_t size = size_t(1) << (marker & 0xf);
        if (offset + 2 + size > m_offsetTable)
            return std::nullopt;
        *start = offset + 2 + size;
        return number(offset + 2, size);
    }

    std::optional<std::string> string(uint64_t object) const
    {
        const std::optional<size_t> offset = objectOffset(object);
        if (!offset)
            return std::nullopt;
        const uint8_t type = uint8_t(m_data[*offset]) >> 4;
        if (type != 0x5 && type != 0x6)
            return std::nullopt;
        size_t start = 0;
        const std::optional<uint64_t> count = length(*offset, &start);
        if (!count)
            return std::nullopt;
        const uint64_t available = m_offsetTable - start;

        if (type == 0x5) {
            if (*count > available)
                return std::nullopt;
            std::string result;
            for (uint64_t i = 0; i < *count; ++i)
                appendUtf8(&result, uint8_t(m_data[start + i]));
            return result;
        }

        if (*count > available / 2)
            return std::nullopt;
        std::string result;
        for (uint64_t i = 0; i < *count; ++i) {
            uint32_t unit = uint32_t(number(start + i * 2, 2));
            if (unit >= 0xd800 && unit < 0xdc00 && i + 1 < *count) {
                const uint32_t low = uint32_t(number(start + (i + 1) * 2, 2));
                if (low >= 0xdc00 && low < 0xe000) {
                    unit = 0x10000 + ((unit - 0xd800) << 10) + (low - 0xdc00);
                    ++i;
                }
            }
            if (unit >= 0xd800 && unit < 0xe000)
                unit = 0xfffd;
            appendUtf8(&result, unit);
        }
        return result;
    }

    std::string_view m_data;
    size_t m_offsetSize = 0;
    size_t m_referenceSize = 0;
    uint64_t m_objectCount = 0;
    uint64_t m_offsetTable = 0;
};

std::optional<Strings> rootStrings(std::string_view data)
{
    if (data.starts_with("bplist00"))
        return BinaryReader(data).read();
    return readXml(data);
}

} // namespace cutefish::base::plist
