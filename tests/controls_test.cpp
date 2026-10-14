// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <filesystem>

#include "ui/check_box.h"
#include "ui/combo_box.h"
#include "ui/icon.h"
#include "ui/keys.h"
#include "ui/layout.h"
#include "ui/menu.h"
#include "ui/segmented_control.h"
#include "ui/slider.h"
#include "ui/switch.h"
#include "ui/test_input.h"
#include "ui/theme.h"
#include "ui/window.h"

using namespace cutefish::ui;

template<typename T>
static T* host(Window& window, std::unique_ptr<T> view, SizeF size = {300, 120})
{
    T* raw = view.get();
    auto column = std::make_unique<Column>();
    column->addChild(std::move(view));
    window.resize(size);
    window.setContentView(std::move(column));
    window.show();
    window.grab();
    TestInput::setKeyboardFocus(window, true);
    return raw;
}

TEST(ControlsTest, SwitchTogglesByPointerAndSpace)
{
    Window window;
    Switch* control = host(window, std::make_unique<Switch>("Wi-Fi"));
    std::vector<bool> changes;
    control->onToggled = [&changes](bool on) { changes.push_back(on); };
    EXPECT_EQ(control->sizeHint().height, 20 + 2 * 8);
    TestInput::click(window, {20, 18});
    control->setFocus();
    TestInput::typeKey(window, key::Space);
    EXPECT_EQ(changes, (std::vector<bool>{true, false}));
    // From code, quietly.
    control->setChecked(true);
    EXPECT_EQ(changes.size(), 2u);
}

TEST(ControlsTest, RadioButtonsTurnEachOtherOff)
{
    Window window;
    ButtonGroup group;
    auto column = std::make_unique<Column>();
    std::vector<RadioButton*> buttons;
    for (const char* text : {"One", "Two", "Three"}) {
        buttons.push_back(column->addChild<RadioButton>(text));
        buttons.back()->setGroup(&group);
    }
    int changed = -1;
    group.onChanged = [&changed](int index) { changed = index; };
    window.resize({200, 100});
    window.setContentView(std::move(column));
    window.show();
    window.grab();

    buttons[0]->setChecked(true);
    TestInput::click(window, {20, buttons[2]->geometry().y + 10});
    EXPECT_FALSE(buttons[0]->isChecked());
    EXPECT_TRUE(buttons[2]->isChecked());
    EXPECT_EQ(changed, 2);
    // Clicking the one that is on leaves it on.
    TestInput::click(window, {20, buttons[2]->geometry().y + 10});
    EXPECT_TRUE(buttons[2]->isChecked());
}

TEST(ControlsTest, SliderSnapsToSteps)
{
    Window window;
    auto slider = std::make_unique<Slider>();
    slider->setRange(0, 10);
    slider->setStepSize(1);
    slider->setLayoutHints({.preferredWidth = 222});
    Slider* control = host(window, std::move(slider));
    double moved = -1;
    control->onMoved = [&moved](double value) { moved = value; };
    // The handle's centre travels 200 pixels, from 11 to 211.
    TestInput::click(window, {11 + 200 * 0.36, 16});
    EXPECT_EQ(control->value(), 4);
    EXPECT_EQ(moved, 4);
    control->setFocus();
    TestInput::typeKey(window, key::Right);
    EXPECT_EQ(control->value(), 5);
    TestInput::typeKey(window, key::End);
    EXPECT_EQ(control->value(), 10);
}

TEST(ControlsTest, SegmentedControlPicksByPointerAndArrows)
{
    Window window;
    SegmentedControl* control =
        host(window, std::make_unique<SegmentedControl>(std::vector<std::string>{"A", "B", "C"}));
    int picked = -1;
    control->onCurrentChanged = [&picked](int index) { picked = index; };
    const RectF bounds = control->geometry();
    TestInput::click(window, {bounds.x + bounds.width * 5 / 6, bounds.y + 10});
    EXPECT_EQ(control->currentIndex(), 2);
    EXPECT_EQ(picked, 2);
    control->setFocus();
    TestInput::typeKey(window, key::Left);
    EXPECT_EQ(control->currentIndex(), 1);
}

TEST(ControlsTest, ComboBoxMenuChecksTheCurrentItem)
{
    Window window;
    ComboBox* box =
        host(window, std::make_unique<ComboBox>(std::vector<std::string>{"Small", "Medium", "Large"}));
    box->setCurrentIndex(1);
    int activated = -1;
    box->onActivated = [&activated](int index) { activated = index; };
    TestInput::click(window, {20, 15});
    // The menu takes the keys and opens on the current item; one down is the last.
    TestInput::typeKey(window, key::Down);
    TestInput::typeKey(window, key::Return);
    EXPECT_EQ(box->currentText(), "Large");
    EXPECT_EQ(activated, 2);
}

TEST(ControlsTest, GlyphsFillTheirSize)
{
    Window window;
    auto icon = std::make_unique<Icon>("check", 16);
    icon->setColor(Color::rgb(0xFF0000));
    host(window, std::move(icon), {16, 16});
    const Image image = window.grab();
    int covered = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixel(x, y).g < 230)
                ++covered;
        }
    }
    // Drawn at its declared 64 pixels, the check would fall outside 16 by 16.
    EXPECT_GT(covered, 30);
}

TEST(ControlsTest, ThemeIconsFollowTheInheritChain)
{
    ThemeIcon icon("folder", 16);
    const std::string path = icon.path();
    if (!std::filesystem::exists("/usr/share/icons/" + Theme::current().iconTheme))
        GTEST_SKIP() << "the icon theme is not installed";
    EXPECT_FALSE(path.empty());
    // An unknown name falls back along its dashes.
    ThemeIcon fallback("folder-some-unknown-kind", 16);
    EXPECT_EQ(fallback.path(), path);
}
