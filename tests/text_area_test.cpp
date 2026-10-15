// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "ui/keys.h"
#include "ui/layout.h"
#include "ui/test_input.h"
#include "ui/text_area.h"
#include "ui/window.h"

using namespace cutefish::ui;

namespace {

struct Hosted
{
    Window window;
    TextArea* area = nullptr;

    explicit Hosted(double scale = 1.5, double width = 200)
    {
        auto root = std::make_unique<Column>();
        area = root->addChild<TextArea>();
        area->setLayoutHints({.preferredWidth = width});
        window.setContentView(std::move(root));
        window.resize({300, 300});
        window.setScale(scale);
        window.show();
        window.grab();
        TestInput::setKeyboardFocus(window, true);
        area->setFocus();
    }
};

} // namespace

TEST(TextAreaTest, TypesLinesAndGrows)
{
    Hosted hosted;
    const double oneLine = hosted.area->geometry().height;
    TestInput::typeText(hosted.window, "First line");
    TestInput::typeKey(hosted.window, key::Return);
    TestInput::typeText(hosted.window, "Second");
    EXPECT_EQ(hosted.area->text(), "First line\nSecond");
    hosted.window.grab();
    EXPECT_GT(hosted.area->geometry().height, oneLine);
    // FishUI's padding of 6 around two lines.
    EXPECT_DOUBLE_EQ(hosted.area->geometry().height, 2 * (oneLine - 12) + 12);
}

TEST(TextAreaTest, UpAndDownKeepTheColumn)
{
    Hosted hosted;
    hosted.area->setText("abcdef\nabcdef\nab");
    hosted.window.grab();
    hosted.area->setCursorPosition(4);
    TestInput::typeKey(hosted.window, key::Down);
    EXPECT_EQ(hosted.area->cursorPosition(), 11u);
    TestInput::typeKey(hosted.window, key::Down);
    // The last line is shorter; the caret goes to its end and comes back.
    EXPECT_EQ(hosted.area->cursorPosition(), 16u);
    TestInput::typeKey(hosted.window, key::Up);
    EXPECT_EQ(hosted.area->cursorPosition(), 11u);
    TestInput::typeKey(hosted.window, key::Up, ShiftModifier);
    EXPECT_EQ(hosted.area->selectedText(), "ef\nabcd");
}

TEST(TextAreaTest, WrapsAnywhereWhenAsked)
{
    Hosted hosted(1.5, 120);
    const std::string command = "/usr/lib/x86_64-linux-gnu/libexec/some-long-helper --option=value";
    hosted.area->setText(command);
    hosted.window.grab();
    const double words = hosted.area->geometry().height;
    hosted.area->setWrapMode(TextArea::WrapMode::WrapAnywhere);
    hosted.window.grab();
    const double anywhere = hosted.area->geometry().height;
    hosted.area->setWrapMode(TextArea::WrapMode::NoWrap);
    hosted.window.grab();
    const double single = hosted.area->geometry().height;
    EXPECT_LT(single, words);
    EXPECT_LE(anywhere, words);
    EXPECT_EQ(hosted.area->text(), command);
}

TEST(TextAreaTest, ReadOnlyTextSelectsButDoesNotChange)
{
    Hosted hosted;
    hosted.area->setText("Read only");
    hosted.area->setReadOnly(true);
    hosted.window.grab();
    TestInput::typeText(hosted.window, "x");
    TestInput::typeKey(hosted.window, key::BackSpace);
    EXPECT_EQ(hosted.area->text(), "Read only");
    TestInput::typeKey(hosted.window, key::letter('a'), ControlModifier);
    EXPECT_EQ(hosted.area->selectedText(), "Read only");
    // A double click selects a word.
    const RectF box = hosted.area->geometry();
    TestInput::click(hosted.window, {box.x + 10, box.y + 10});
    TestInput::click(hosted.window, {box.x + 10, box.y + 10});
    EXPECT_EQ(hosted.area->selectedText(), "Read");
}

TEST(TextAreaTest, InputMethodsComposeInPlace)
{
    Hosted hosted;
    TestInput::typeText(hosted.window, "ab");
    TestInput::inputMethod(hosted.window, {.preedit = "xy", .preeditCursorBegin = 2});
    EXPECT_EQ(hosted.area->preedit(), "xy");
    EXPECT_EQ(hosted.area->text(), "ab");
    hosted.window.grab();
    TestInput::inputMethod(hosted.window, {.commit = "XY"});
    EXPECT_EQ(hosted.area->text(), "abXY");
    EXPECT_TRUE(hosted.area->preedit().empty());
}
