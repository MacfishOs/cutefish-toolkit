// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "base/json.h"

using namespace cutefish::base::json;

TEST(JsonTest, ReadsDocuments)
{
    const std::optional<Value> value = Value::parse(
        R"( {"name": "Copy", "keys": ["Ctrl", "C"], "enabled": true, "id": 42, "ratio": -1.5e2, "none": null} )");
    ASSERT_TRUE(value);
    EXPECT_EQ((*value)["name"].toString(), "Copy");
    EXPECT_EQ((*value)["keys"].size(), 2u);
    EXPECT_EQ((*value)["keys"][1].toString(), "C");
    EXPECT_TRUE((*value)["enabled"].toBool());
    EXPECT_EQ((*value)["id"].toInt(), 42);
    EXPECT_DOUBLE_EQ((*value)["ratio"].toDouble(), -150);
    EXPECT_TRUE((*value)["none"].isNull());
    EXPECT_TRUE(value->contains("none"));
    EXPECT_FALSE(value->contains("missing"));
    EXPECT_TRUE((*value)["missing"].isNull());
}

TEST(JsonTest, ReadsEscapes)
{
    const std::optional<Value> value = Value::parse(R"(["a\"b\\c\/d\n", "\u00e9\ud83d\ude00"])");
    ASSERT_TRUE(value);
    EXPECT_EQ((*value)[0].toString(), "a\"b\\c/d\n");
    EXPECT_EQ((*value)[1].toString(), "\u00e9\U0001F600");
}

TEST(JsonTest, RefusesInvalidText)
{
    for (const char* text : {"", "{", "[1,]", "{\"a\" 1}", "01", "1.", "\"\\x\"", "\"\\ud800\"", "nul", "[1] 2",
                             "\"a\nb\""})
        EXPECT_FALSE(Value::parse(text)) << text;
}

TEST(JsonTest, SerializesCompactOrIndented)
{
    Value value = Value::object();
    value.insert("text", "tab\there \"quoted\"");
    value.insert("whole", 3.0);
    value.insert("fraction", 0.1);
    Value list;
    list.append(true);
    list.append(nullptr);
    value.insert("list", list);
    EXPECT_EQ(value.serialize(), R"({"text":"tab\there \"quoted\"","whole":3,"fraction":0.1,"list":[true,null]})");
    EXPECT_EQ(Value::parse(value.serialize()), value);
    EXPECT_EQ(Value(Value::Array{}).serialize(false), "[\n]\n");
    EXPECT_EQ(Value::object().serialize(false), "{\n}\n");
    EXPECT_EQ(Value::object().serialize(), "{}");
    // Empty containers as display.json has them.
    EXPECT_EQ(Value::parse(R"({"e":{},"x":{"b":[],"d":1.5}})")->serialize(false),
              "{\n    \"e\": {\n    },\n    \"x\": {\n        \"b\": [\n        ],\n        \"d\": 1.5\n    }\n}\n");
    EXPECT_EQ(Value::parse("{\"a\":[1]}")->serialize(false), "{\n    \"a\": [\n        1\n    ]\n}\n");
}
