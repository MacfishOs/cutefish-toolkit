// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <cmath>

#include "ui/clipboard.h"
#include "ui/keys.h"
#include "ui/layout.h"
#include "ui/search_field.h"
#include "ui/test_input.h"
#include "ui/text_field.h"
#include "ui/window.h"

using namespace cutefish::ui;

// e and a combining acute accent: one grapheme of three bytes.
static const std::string kAccented = "é";
// A family emoji joined by zero-width joiners: one grapheme of 18 bytes.
static const std::string kFamily = "\U0001F468‍\U0001F469‍\U0001F467";

namespace {

struct Fixture
{
    explicit Fixture(std::string text = {})
    {
        auto content = std::make_unique<Column>();
        content->setMargins(Margins::all(10));
        field = content->addChild<TextField>(std::move(text));
        field->setLayoutHints({.fillWidth = true});
        window.resize({300, 60});
        window.setContentView(std::move(content));
        window.show();
        window.grab();
        TestInput::setKeyboardFocus(window, true);
        field->setFocus();
    }

    void key(uint32_t key, uint32_t modifiers = NoModifier)
    {
        TestInput::typeKey(window, key, modifiers);
    }

    Window window;
    TextField* field;
};

} // namespace

TEST(TextFieldTest, TypesAndDeletesWholeGraphemes)
{
    Fixture f;
    TestInput::typeText(f.window, "caf" + kAccented + " " + kFamily);
    EXPECT_EQ(f.field->text(), "caf" + kAccented + " " + kFamily);

    f.key(key::BackSpace);
    EXPECT_EQ(f.field->text(), "caf" + kAccented + " ");
    f.key(key::BackSpace);
    f.key(key::BackSpace);
    EXPECT_EQ(f.field->text(), "caf");

    f.key(key::Left);
    f.key(key::Delete);
    EXPECT_EQ(f.field->text(), "ca");
    EXPECT_EQ(f.field->cursorPosition(), 2u);
}

TEST(TextFieldTest, MovesAndDeletesByWord)
{
    Fixture f("open the door");
    f.key(key::Left, ControlModifier);
    EXPECT_EQ(f.field->cursorPosition(), 9u);
    f.key(key::Left, ControlModifier);
    EXPECT_EQ(f.field->cursorPosition(), 5u);
    f.key(key::Right, ControlModifier);
    EXPECT_EQ(f.field->cursorPosition(), 8u);
    f.key(key::End);
    f.key(key::BackSpace, ControlModifier);
    EXPECT_EQ(f.field->text(), "open the ");
}

TEST(TextFieldTest, SelectsAndReplaces)
{
    Fixture f("hello world");
    f.key(key::Home);
    f.key(key::Right, ShiftModifier | ControlModifier);
    EXPECT_EQ(f.field->selectedText(), "hello");
    TestInput::typeText(f.window, "J");
    EXPECT_EQ(f.field->text(), "J world");

    f.key(key::letter('a'), ControlModifier);
    EXPECT_EQ(f.field->selectedText(), "J world");
    // An arrow drops the selection at its edge.
    f.key(key::Left);
    EXPECT_FALSE(f.field->hasSelection());
    EXPECT_EQ(f.field->cursorPosition(), 0u);
}

TEST(TextFieldTest, TypingUndoesAsOneStep)
{
    Fixture f;
    TestInput::typeText(f.window, "abc");
    f.key(key::Left);
    TestInput::typeText(f.window, "x");
    EXPECT_EQ(f.field->text(), "abxc");

    f.key(key::letter('z'), ControlModifier);
    EXPECT_EQ(f.field->text(), "abc");
    f.key(key::letter('z'), ControlModifier);
    EXPECT_EQ(f.field->text(), "");
    f.key(key::letter('z'), ControlModifier | ShiftModifier);
    EXPECT_EQ(f.field->text(), "abc");
    EXPECT_EQ(f.field->cursorPosition(), 3u);
}

TEST(TextFieldTest, CopiesAndPastes)
{
    Fixture f("one two");
    f.key(key::Left, ShiftModifier | ControlModifier);
    f.key(key::letter('x'), ControlModifier);
    EXPECT_EQ(f.field->text(), "one ");
    f.key(key::Home);
    f.key(key::letter('v'), ControlModifier);
    EXPECT_EQ(f.field->text(), "twoone ");

    // Line breaks in pasted text become spaces.
    Clipboard::setText("a\nb\r\n");
    f.key(key::End);
    f.key(key::letter('v'), ControlModifier);
    EXPECT_EQ(f.field->text(), "twoone a b ");
}

TEST(TextFieldTest, PasswordsStayOffTheClipboard)
{
    Fixture f("secret");
    f.field->setEchoMode(TextField::EchoMode::Password);
    Clipboard::setText("before");
    f.key(key::letter('a'), ControlModifier);
    f.key(key::letter('c'), ControlModifier);
    f.key(key::letter('x'), ControlModifier);
    EXPECT_EQ(f.field->text(), "secret");
    std::string clipboard;
    Clipboard::requestText([&clipboard](std::string text) { clipboard = text; });
    EXPECT_EQ(clipboard, "before");
    // Grabbing paints the dots without trouble.
    EXPECT_FALSE(f.window.grab().isNull());
}

TEST(TextFieldTest, InputMethodComposesAtTheCursor)
{
    Fixture f("ab");
    f.key(key::Left);

    TextInputEvent compose;
    compose.preedit = "ni";
    compose.preeditCursorBegin = 2;
    TestInput::inputMethod(f.window, compose);
    EXPECT_EQ(f.field->text(), "ab");
    EXPECT_EQ(f.field->preedit(), "ni");

    TextInputEvent commit;
    commit.commit = "XY";
    TestInput::inputMethod(f.window, commit);
    EXPECT_EQ(f.field->text(), "aXYb");
    EXPECT_EQ(f.field->preedit(), "");
    EXPECT_EQ(f.field->cursorPosition(), 3u);

    TextInputEvent replace;
    replace.deleteBefore = 2;
    replace.deleteAfter = 1;
    replace.commit = "Z";
    TestInput::inputMethod(f.window, replace);
    EXPECT_EQ(f.field->text(), "aZ");
}

TEST(TextFieldTest, ReadOnlyFieldsMoveButDoNotEdit)
{
    Fixture f("fixed");
    f.field->setReadOnly(true);
    TestInput::typeText(f.window, "x");
    f.key(key::BackSpace);
    EXPECT_EQ(f.field->text(), "fixed");
    f.key(key::Home);
    EXPECT_EQ(f.field->cursorPosition(), 0u);
}

TEST(TextFieldTest, DoubleClickSelectsAWord)
{
    Fixture f("alpha beta");
    // Inside the first word, past the twelve-pixel padding.
    TestInput::click(f.window, {30, 27});
    TestInput::click(f.window, {30, 27});
    EXPECT_EQ(f.field->selectedText(), "alpha");
}

TEST(TextFieldTest, RendersAtFractionalScales)
{
    for (double scale : {1.0, 1.25, 1.5, 1.75}) {
        Fixture f("Some text " + kFamily);
        f.window.setScale(scale);
        f.key(key::letter('a'), ControlModifier);
        const Image image = f.window.grab();
        EXPECT_EQ(image.width(), int(std::lround(300 * scale)));
    }
}

// A maximum length and a validator: what does not fit or is refused does not
// happen; textEdited is the user's alone; editingFinished on Return.
TEST(TextFieldTest, LimitsValidatesAndReportsEdits)
{
    Fixture f;
    f.field->setMaximumLength(4);
    TestInput::typeText(f.window, "abcdef");
    EXPECT_EQ(f.field->text(), "abcd");

    f.field->setText("");
    f.field->setMaximumLength(0);
    f.field->setValidator([](const std::string& text) {
        return text.find_first_not_of("0123456789") == std::string::npos;
    });
    int edits = 0;
    int finished = 0;
    f.field->onTextEdited = [&edits] { ++edits; };
    f.field->onEditingFinished = [&finished] { ++finished; };
    TestInput::typeText(f.window, "1a2");
    EXPECT_EQ(f.field->text(), "12");
    EXPECT_EQ(edits, 2);
    f.field->setText("7");
    EXPECT_EQ(edits, 2);
    f.key(key::Return);
    EXPECT_EQ(finished, 1);
}

// The clear button shows with text, clears it on a click, and the text
// margins follow it: the glyph, the gap and the inset.
TEST(TextFieldTest, SearchFieldClears)
{
    Window window;
    auto column = std::make_unique<Column>();
    SearchField* field = column->addChild<SearchField>("Search");
    field->setLayoutHints({.preferredWidth = 220, .preferredHeight = 30});
    window.resize({300, 60});
    window.setContentView(std::move(column));
    window.show();
    window.grab();
    TestInput::setKeyboardFocus(window, true);

    const RectF bounds = field->geometry();
    TestInput::click(window, {bounds.x + 100, bounds.y + 15});
    EXPECT_TRUE(field->hasFocus());
    TestInput::typeText(window, "abc");
    window.grab();
    std::string changed;
    field->onTextChanged = [&changed, field] { changed = field->text() + "!"; };
    // The clear glyph is 14 wide, 12 from the end; its hit area reaches 7 past it.
    TestInput::click(window, {bounds.right() - 12 - 14 - 5, bounds.y + 15});
    EXPECT_EQ(field->text(), "");
    EXPECT_EQ(changed, "!");
    // Gone without text: a click there lands in the field.
    TestInput::typeText(window, "x");
    EXPECT_EQ(field->text(), "x");
}

TEST(TextFieldTest, MouseSelectionGoesToThePrimarySelection)
{
    Fixture f("alpha beta");
    Clipboard::setText("old", ClipboardMode::Selection);
    TestInput::click(f.window, {30, 27});
    TestInput::click(f.window, {30, 27});
    std::string selected;
    Clipboard::requestText([&](std::string text) { selected = std::move(text); }, ClipboardMode::Selection);
    EXPECT_EQ(selected, "alpha");

    // A middle click pastes it at the cursor, the selection given up.
    Clipboard::setText("X", ClipboardMode::Selection);
    f.key(key::End);
    TestInput::click(f.window, {30, 27}, MouseButton::Middle);
    EXPECT_EQ(f.field->text(), "alpha betaX");
}

TEST(TextFieldTest, PasswordsStayOffThePrimarySelection)
{
    Fixture f("secret");
    f.field->setEchoMode(TextField::EchoMode::Password);
    Clipboard::setText("before", ClipboardMode::Selection);
    TestInput::click(f.window, {30, 27});
    TestInput::click(f.window, {30, 27});
    TestInput::click(f.window, {30, 27});
    std::string selected;
    Clipboard::requestText([&](std::string text) { selected = std::move(text); }, ClipboardMode::Selection);
    EXPECT_EQ(selected, "before");
}
