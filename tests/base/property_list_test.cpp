// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/property_list.h"

using namespace cutefish::base;

static void putNumber(std::string* out, uint64_t value, int size)
{
    for (int i = size - 1; i >= 0; --i)
        *out += char((value >> (8 * i)) & 0xff);
}

// A binary property list of one dictionary; a value that is an integer is
// written as one, a value of more than one byte per unit as UTF-16.
struct Value
{
    std::string text;
    std::u16string wide;
    int number = -1;
};

static std::string binaryPlist(const std::vector<std::pair<std::string, Value>>& members)
{
    std::vector<std::string> objects;
    std::string dictionary(1, char(0xd0 | members.size()));
    for (size_t i = 0; i < members.size(); ++i)
        dictionary += char(1 + i);
    for (size_t i = 0; i < members.size(); ++i)
        dictionary += char(1 + members.size() + i);
    objects.push_back(dictionary);

    const auto ascii = [](const std::string& text) {
        std::string object;
        if (text.size() < 15) {
            object += char(0x50 | text.size());
        } else {
            object += char(0x5f);
            object += char(0x10);
            object += char(text.size());
        }
        return object + text;
    };
    for (const auto& [key, value] : members)
        objects.push_back(ascii(key));
    for (const auto& [key, value] : members) {
        if (value.number >= 0) {
            objects.push_back(std::string(1, char(0x10)) + char(value.number));
        } else if (!value.wide.empty()) {
            std::string object(1, char(0x60 | value.wide.size()));
            for (const char16_t unit : value.wide)
                putNumber(&object, unit, 2);
            objects.push_back(object);
        } else {
            objects.push_back(ascii(value.text));
        }
    }

    std::string data = "bplist00";
    std::vector<size_t> offsets;
    for (const std::string& object : objects) {
        offsets.push_back(data.size());
        data += object;
    }
    const size_t table = data.size();
    for (const size_t offset : offsets)
        data += char(offset);
    data += std::string(6, '\0');
    data += char(1);
    data += char(1);
    putNumber(&data, objects.size(), 8);
    putNumber(&data, 0, 8);
    putNumber(&data, table, 8);
    return data;
}

TEST(PropertyListTest, ReadsTheRootStringsOfXml)
{
    const auto strings = plist::rootStrings(R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<!-- A comment -->
	<key>CFBundleName</key>
	<string>Tom &amp; Jerry &#x263A;</string>
	<key>CFBundleDocumentTypes</key>
	<array>
		<dict>
			<key>CFBundleTypeName</key>
			<string>Nested</string>
		</dict>
	</array>
	<key>LSRequiresNativeExecution</key>
	<true/>
	<key>CFBundleIconFile</key>
	<string/>
	<key>CFBundleIdentifier</key>
	<string><![CDATA[org.example.<App>]]></string>
</dict>
</plist>
)");
    ASSERT_TRUE(strings);
    EXPECT_EQ(strings->size(), 3u);
    EXPECT_EQ(strings->at("CFBundleName"), "Tom & Jerry ☺");
    EXPECT_EQ(strings->at("CFBundleIconFile"), "");
    EXPECT_EQ(strings->at("CFBundleIdentifier"), "org.example.<App>");
    EXPECT_FALSE(strings->contains("CFBundleTypeName"));
}

TEST(PropertyListTest, ReadsTheRootStringsOfABinaryList)
{
    const std::string data = binaryPlist({
            {"CFBundleName", {.text = "Editor"}},
            {"CFBundleDisplayName", {.wide = u"Café \U0001F600"}},
            {"CFBundleExecutableTooLong", {.text = "a name longer than fifteen"}},
            {"Count", {.number = 5}},
    });
    const auto strings = plist::rootStrings(data);
    ASSERT_TRUE(strings);
    EXPECT_EQ(strings->size(), 3u);
    EXPECT_EQ(strings->at("CFBundleName"), "Editor");
    EXPECT_EQ(strings->at("CFBundleDisplayName"), "Café \U0001F600");
    EXPECT_EQ(strings->at("CFBundleExecutableTooLong"), "a name longer than fifteen");
}

TEST(PropertyListTest, RefusesWhatIsNoDictionary)
{
    EXPECT_FALSE(plist::rootStrings(""));
    EXPECT_FALSE(plist::rootStrings("<plist><array><string>a</string></array></plist>"));
    EXPECT_FALSE(plist::rootStrings("<plist><dict><key>a</key><string>b</dict></plist>"));
    EXPECT_FALSE(plist::rootStrings("<plist><dict><key>a</key><string>&bogus;</string></dict></plist>"));

    std::string data = binaryPlist({{"CFBundleName", {.text = "Editor"}}});
    EXPECT_TRUE(plist::rootStrings(data));
    // Every truncation, and an offset table that points past the objects.
    for (size_t size = 0; size < data.size(); ++size)
        EXPECT_FALSE(plist::rootStrings(data.substr(0, size))) << size;
    data[data.size() - 1] = char(0xff);
    EXPECT_FALSE(plist::rootStrings(data));
}
