// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "ui/dialog_window.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/layout.h"
#include "ui/test_input.h"
#include "ui/theme.h"
#include "ui/toast.h"
#include "ui/window.h"
#include "ui/window_header.h"

using namespace cutefish::ui;

TEST(WindowTest, HeaderDoubleClickTogglesMaximized)
{
    Window window;
    auto column = std::make_unique<Column>();
    WindowHeader* header = column->addChild<WindowHeader>();
    header->setLayoutHints({.fillWidth = true});
    window.resize({400, 300});
    window.setContentView(std::move(column));
    window.show();
    window.grab();
    int changes = 0;
    window.onStatesChanged = [&changes] { ++changes; };

    TestInput::click(window, {100, 20});
    TestInput::click(window, {100, 20});
    EXPECT_TRUE(window.states().maximized);
    EXPECT_EQ(changes, 1);
    // The maximize button, second from the end, restores.
    TestInput::click(window, {400 - 4 - 32 - 1 - 16, 20});
    EXPECT_FALSE(window.states().maximized);
}

TEST(WindowTest, ConfirmDialogAnswersOnce)
{
    Window parent;
    parent.resize({400, 300});
    parent.setContentView(std::make_unique<View>());
    parent.show();

    ConfirmDialog dialog(&parent, "Remove the file?", "It cannot be brought back.");
    dialog.setDestructive(true);
    int accepted = 0;
    int rejected = 0;
    dialog.onAccepted = [&accepted] { ++accepted; };
    dialog.onRejected = [&rejected] { ++rejected; };
    dialog.show();
    Window* window = dialog.window();
    ASSERT_NE(window, nullptr);
    window->grab();
    EXPECT_GE(window->size().width, 336);
    // A destructive dialog starts on Cancel; Escape cancels too.
    TestInput::setKeyboardFocus(*window, true);
    TestInput::typeKey(*window, key::Escape);
    EXPECT_EQ(rejected, 1);
    EXPECT_EQ(accepted, 0);
}

// As FishUI's ConfirmDialog: Return confirms even while a destructive
// dialog's Cancel has the focus; Space presses the focused button.
TEST(WindowTest, ConfirmDialogReturnConfirms)
{
    Window parent;
    parent.resize({400, 300});
    parent.setContentView(std::make_unique<View>());
    parent.show();

    for (const bool space : {false, true}) {
        ConfirmDialog dialog(&parent, "Remove the file?", "It cannot be brought back.");
        dialog.setDestructive(true);
        int accepted = 0;
        int rejected = 0;
        dialog.onAccepted = [&accepted] { ++accepted; };
        dialog.onRejected = [&rejected] { ++rejected; };
        dialog.show();
        Window* window = dialog.window();
        window->grab();
        TestInput::setKeyboardFocus(*window, true);
        TestInput::typeKey(*window, space ? key::Space : key::Return);
        EXPECT_EQ(accepted, space ? 0 : 1);
        EXPECT_EQ(rejected, space ? 1 : 0);
    }
}

TEST(WindowTest, ConfirmDialogTextChangesWhileShown)
{
    Window parent;
    parent.resize({400, 300});
    parent.setContentView(std::make_unique<View>());
    parent.show();

    ConfirmDialog dialog(&parent, "Pairing");
    dialog.show();
    Window* window = dialog.window();
    window->grab();
    const double height = window->size().height;
    dialog.setText("Confirm that the code 123456 shows on the other device.");
    window->grab();
    EXPECT_GT(window->size().height, height);
    dialog.setTitle("Pairing with the keyboard");
    const View* texts = window->contentView()->children().front().get();
    EXPECT_EQ(static_cast<const Label*>(texts->children().front().get())->text(), "Pairing with the keyboard");
    dialog.setText({});
    window->grab();
    EXPECT_EQ(window->size().height, height);
}

TEST(WindowTest, DialogTakesTheHeightOfItsContent)
{
    Window parent;
    parent.resize({400, 300});
    parent.setContentView(std::make_unique<View>());
    parent.show();

    DialogWindow dialog(&parent, "Details", 320);
    auto column = std::make_unique<Column>();
    column->addChild<View>()->setLayoutHints({.preferredHeight = 100});
    dialog.setContentView(std::move(column));
    dialog.show();
    // A 44-pixel header, the content, 16 below it.
    EXPECT_EQ(dialog.window()->size().height, 44 + 100 + 16);
    EXPECT_EQ(dialog.window()->size().width, 320);
}

TEST(WindowTest, ThemeChangesReachLabels)
{
    Window window;
    auto label = std::make_unique<Label>("Theme");
    label->setFontSize(30);
    window.resize({100, 50});
    window.setContentView(std::move(label));
    window.show();

    const auto darkest = [](const Image& image) {
        int value = 255;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x)
                value = std::min(value, int(image.pixel(x, y).g));
        }
        return value;
    };
    const Theme original = Theme::current();
    EXPECT_LT(darkest(window.grab()), 100);
    Theme::setCurrent(Theme::dark());
    // White text on the dark background: nothing dark but the background.
    EXPECT_EQ(darkest(window.grab()), Theme::dark().background.g);
    Theme::setCurrent(original);
}

TEST(WindowTest, DialogKeepsItsMarginAtEveryScale)
{
    Window parent;
    parent.resize({760, 720});
    parent.setContentView(std::make_unique<View>());
    parent.setScale(1.5);
    parent.show();

    ConfirmDialog dialog(&parent, "Move the file to the trash?", "It can be put back from the trash later.");
    dialog.show();
    Window* window = dialog.window();
    EXPECT_EQ(window->scale(), 1.5);
    // Text lines are taller at 150%; the window grows instead of the margin shrinking.
    for (const double scale : {1.5, 1.0, 1.25}) {
        window->setScale(scale);
        window->grab();
        const View* actions = window->contentView()->children().back().get();
        const double margin = window->size().height - actions->geometry().bottom();
        EXPECT_GE(margin, 12) << scale;
        EXPECT_LT(margin, 13) << scale;
    }
}

TEST(WindowTest, DialogFitsTextLinesAtFractionalScale)
{
    Window parent;
    parent.resize({400, 300});
    parent.setContentView(std::make_unique<View>());
    parent.setScale(1.5);
    parent.show();

    DialogWindow dialog(&parent, "Network details", 360);
    auto column = std::make_unique<Column>();
    column->setSpacing(8);
    for (const char* line : {"Address 192.168.1.20", "Gateway 192.168.1.1", "DNS 1.1.1.1"})
        column->addChild<Label>(line);
    View* content = column.get();
    dialog.setContentView(std::move(column));
    dialog.show();
    Window* window = dialog.window();
    window->grab();
    const double bottom = content->geometry().y + content->children().back()->geometry().bottom();
    EXPECT_GE(window->size().height - bottom, 16);
    EXPECT_LT(window->size().height - bottom, 17);
}

// As QWindow::isActive: a window stays active while its dialog has the focus.
TEST(WindowTest, ParentStaysActiveWhileItsDialogIsFocused)
{
    Window parent;
    parent.resize({200, 100});
    parent.show();
    Window dialog;
    dialog.setTransientParent(&parent, true);
    dialog.resize({100, 50});
    dialog.show();
    Window other;
    other.resize({100, 50});
    other.show();

    // Headless windows show activated.
    TestInput::configure(dialog, 100, 50, {});
    TestInput::configure(other, 100, 50, {});
    TestInput::configure(parent, 200, 100, {.activated = true});
    EXPECT_TRUE(parent.isActive());
    EXPECT_TRUE(dialog.isActive());
    EXPECT_FALSE(other.isActive());

    TestInput::configure(parent, 200, 100, {});
    TestInput::configure(dialog, 100, 50, {.activated = true});
    EXPECT_TRUE(parent.isActive());
    EXPECT_TRUE(dialog.isActive());

    TestInput::configure(dialog, 100, 50, {});
    TestInput::configure(other, 100, 50, {.activated = true});
    EXPECT_FALSE(parent.isActive());
    EXPECT_FALSE(dialog.isActive());
    EXPECT_TRUE(other.isActive());
}

// FishUI's Toast: over the content at the bottom centre once faded in.
TEST(WindowTest, ToastShowsOverTheContent)
{
    Window window;
    window.resize({400, 300});
    window.setContentView(std::make_unique<View>());
    window.show();
    window.grab();
    const Image before = window.grab(1'000'000'000);

    Toast toast(&window);
    toast.show("Saved");
    window.grab(1'000'000'000 + 500'000'000);
    const Image shown = window.grab(1'000'000'000 + 600'000'000);
    int changed = 0;
    for (int y = 0; y < 300; ++y) {
        for (int x = 0; x < 400; ++x) {
            if (shown.pixel(x, y).argb() != before.pixel(x, y).argb()) {
                ++changed;
                // Only near the bottom, centred.
                EXPECT_GT(y, 200);
            }
        }
    }
    EXPECT_GT(changed, 0);
}
