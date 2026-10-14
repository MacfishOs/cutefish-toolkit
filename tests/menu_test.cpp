// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "test_views.h"
#include "ui/keys.h"
#include "ui/layout.h"
#include "ui/menu.h"
#include "ui/test_input.h"
#include "ui/text_field.h"
#include "ui/window.h"

using namespace cutefish::ui;

namespace {

struct Fixture
{
    Fixture()
    {
        auto content = std::make_unique<Column>();
        anchor = content->addChild<test::Box>(SizeF{80, 30});
        window.resize({400, 300});
        window.setContentView(std::move(content));
        window.show();
        window.grab();

        menu.addItem("Open", [this] { chosen.push_back("Open"); }, "Ctrl+O");
        disabled = menu.addItem("Save", [this] { chosen.push_back("Save"); });
        menu.setItemEnabled(disabled, false);
        menu.addSeparator();
        menu.addItem("Quit", [this] { chosen.push_back("Quit"); });
        menu.onClosed = [this] { ++closed; };
    }

    Window window;
    test::Box* anchor;
    Menu menu;
    size_t disabled = 0;
    std::vector<std::string> chosen;
    int closed = 0;
};

} // namespace

TEST(MenuTest, KeyboardSkipsDisabledRowsAndSeparators)
{
    Fixture f;
    f.menu.popup(&f.window, {20, 20});
    ASSERT_TRUE(f.menu.isOpen());
    Window* popup = f.menu.window();
    popup->grab();
    TestInput::setKeyboardFocus(*popup, true);
    TestInput::typeKey(*popup, key::Down);
    TestInput::typeKey(*popup, key::Down);
    TestInput::typeKey(*popup, key::Return);
    EXPECT_EQ(f.chosen, (std::vector<std::string>{"Quit"}));
    EXPECT_FALSE(f.menu.isOpen());
    EXPECT_EQ(f.closed, 1);
}

TEST(MenuTest, RowsFollowFishUIMetrics)
{
    Fixture f;
    f.menu.popupFor(f.anchor);
    Window* popup = f.menu.window();
    popup->grab();
    // Three rows of 30, one separator of 5, five pixels above and below.
    EXPECT_EQ(popup->size().height, 3 * 30 + 5 + 2 * 5);
    EXPECT_GE(popup->size().width, 146 + 2 * 3);
}

TEST(MenuTest, ClickChoosesTheRowUnderThePointer)
{
    Fixture f;
    f.menu.popup(&f.window, {20, 20});
    Window* popup = f.menu.window();
    popup->grab();
    // The second row is disabled; a click there does nothing.
    TestInput::click(*popup, {40, 5 + 30 + 15});
    EXPECT_TRUE(f.chosen.empty());
    EXPECT_TRUE(f.menu.isOpen());
    TestInput::click(*popup, {40, 5 + 15});
    EXPECT_EQ(f.chosen, (std::vector<std::string>{"Open"}));
}

TEST(MenuTest, EscapeAndDismissalClose)
{
    Fixture f;
    f.menu.popup(&f.window, {20, 20});
    TestInput::typeKey(*f.menu.window(), key::Escape);
    EXPECT_FALSE(f.menu.isOpen());
    EXPECT_TRUE(f.chosen.empty());
}

TEST(MenuTest, TextFieldOffersItsEditingMenu)
{
    Window window;
    auto content = std::make_unique<Column>();
    TextField* field = content->addChild<TextField>("hello");
    field->setLayoutHints({.fillWidth = true});
    window.resize({300, 100});
    window.setContentView(std::move(content));
    window.show();
    window.grab();
    TestInput::setKeyboardFocus(window, true);

    TestInput::click(window, {30, 15});
    TestInput::typeKey(window, key::letter('a'), ControlModifier);
    TestInput::press(window, {30, 15}, MouseButton::Right);
    TestInput::release(window, {30, 15}, MouseButton::Right);
    // The field keeps its selection while the menu is up.
    EXPECT_EQ(field->selectedText(), "hello");
}
