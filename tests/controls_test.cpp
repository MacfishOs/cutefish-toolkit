// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

#include "ui/check_box.h"
#include "ui/combo_box.h"
#include "ui/header_button.h"
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

// FishUI's paddings: 4 on every side of a check box or radio button, the
// indicator at the left; a switch's 8 only across without text, and its
// indicator centred then.
TEST(ControlsTest, TogglesKeepFishUIPadding)
{
    Window window;
    auto column = std::make_unique<Column>();
    CheckBox* check = column->addChild<CheckBox>();
    RadioButton* radio = column->addChild<RadioButton>("Radio");
    Switch* toggle = column->addChild<Switch>();
    window.resize({300, 200});
    window.setContentView(std::move(column));
    window.show();
    window.grab();
    EXPECT_EQ(check->sizeHint().width, 28);
    EXPECT_EQ(check->sizeHint().height, 28);
    EXPECT_EQ(toggle->sizeHint().width, 54);
    EXPECT_EQ(toggle->sizeHint().height, 20);
    const double width = radio->sizeHint().width;
    // As horizontalPadding: 0.
    radio->setPadding({0, 4, 0, 4});
    EXPECT_EQ(radio->sizeHint().width, width - 8);
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

TEST(ControlsTest, SliderSnapModes)
{
    Window window;
    auto slider = std::make_unique<Slider>();
    slider->setRange(0, 10);
    slider->setStepSize(1);
    slider->setLayoutHints({.preferredWidth = 222});
    Slider* control = host(window, std::move(slider));
    std::vector<bool> pressed;
    control->onPressedChanged = [&pressed, control] { pressed.push_back(control->isPressed()); };
    // Unstepped: as tall as FishUI's, no marks, and the handle follows the
    // pointer between steps until it is let go.
    EXPECT_EQ(control->geometry().height, 27);
    TestInput::press(window, {11 + 200 * 0.36, 13});
    EXPECT_EQ(control->value(), 4);
    EXPECT_EQ(pressed, (std::vector<bool>{true}));
    TestInput::movePointer(window, {11 + 200 * 0.53, 13});
    EXPECT_EQ(control->value(), 5);
    const Image dragged = window.grab();
    TestInput::release(window, {11 + 200 * 0.53, 13});
    const Image landed = window.grab();
    EXPECT_EQ(pressed, (std::vector<bool>{true, false}));
    EXPECT_FALSE(std::equal(dragged.bits(), dragged.bits() + size_t(dragged.width()) * dragged.height(),
                            landed.bits()));

    control->setStepped(true);
    window.grab();
    EXPECT_EQ(control->geometry().height, 32);
}

// The value over the handle while it is held; below it with no room above.
TEST(ControlsTest, SliderShowsItsValueWhileHeld)
{
    Window window;
    auto slider = std::make_unique<Slider>();
    slider->setRange(0, 100);
    slider->setStepSize(1);
    slider->setLayoutHints({.preferredWidth = 222});
    Slider* control = host(window, std::move(slider));
    std::vector<std::string> texts;
    control->setValueToolTip([&texts](double value) {
        texts.push_back(std::to_string(int(value)));
        return texts.back();
    });
    const Image idle = window.grab();
    const auto below = [&window, &idle] {
        const Image image = window.grab();
        int changed = 0;
        // Below the handle's shadow.
        for (int y = 36; y < 80; ++y) {
            for (int x = 0; x < 300; ++x)
                changed += image.pixel(x, y).argb() != idle.pixel(x, y).argb();
        }
        return changed;
    };
    TestInput::press(window, {11 + 200 * 0.5, 13});
    EXPECT_GT(below(), 0);
    TestInput::movePointer(window, {11 + 200 * 0.75, 13});
    EXPECT_GT(below(), 0);
    EXPECT_EQ(texts.back(), "75");
    TestInput::release(window, {11 + 200 * 0.75, 13});
    EXPECT_EQ(below(), 0);
}

// Each button a Tab stop of its own, Space picking it on release, as
// FishUI's TabButtons in a TabBar.
TEST(ControlsTest, SegmentedControlPicksByPointerAndSpace)
{
    Window window;
    SegmentedControl* control =
        host(window, std::make_unique<SegmentedControl>(std::vector<std::string>{"A", "B", "C"}));
    EXPECT_EQ(control->sizeHint().height, 36);
    int picked = -1;
    control->onCurrentChanged = [&picked](int index) { picked = index; };
    const RectF bounds = control->geometry();
    TestInput::click(window, {bounds.x + bounds.width / 6, bounds.y + 10});
    EXPECT_EQ(control->currentIndex(), 0);
    TestInput::click(window, {bounds.x + bounds.width * 5 / 6, bounds.y + 10});
    EXPECT_EQ(control->currentIndex(), 2);
    EXPECT_EQ(picked, 2);

    // The click focused the third button; Shift+Tab goes to the second.
    TestInput::typeKey(window, key::Tab, ShiftModifier);
    TestInput::pressKey(window, {.key = key::Space});
    EXPECT_EQ(control->currentIndex(), 2);
    TestInput::releaseKey(window, {.key = key::Space});
    EXPECT_EQ(control->currentIndex(), 1);
    EXPECT_EQ(picked, 1);
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

// FishUI's ComboBox: the disabled text is disabledTextColor at FishUI.Label's 0.6.
TEST(ControlsTest, ComboBoxGreysItsTextWhenDisabled)
{
    Window window;
    ComboBox* box = host(window, std::make_unique<ComboBox>(std::vector<std::string>{"None"}));
    const auto darkest = [&window, box] {
        const Image image = window.grab();
        int value = 255;
        for (int y = 0; y < int(box->geometry().height); ++y) {
            for (int x = 0; x < int(box->geometry().width) / 2; ++x)
                value = std::min(value, int(image.pixel(x, y).g));
        }
        return value;
    };
    const Theme& theme = Theme::current();
    EXPECT_NEAR(darkest(), theme.text.g, 6);
    box->setEnabled(false);
    EXPECT_NEAR(darkest(), theme.disabledText.g * 0.6 + theme.alternateBackground.g * 0.4, 6);
}

// The pressed plate while checked, as while the button's menu is open, in
// the alphas a header gives it.
TEST(ControlsTest, HeaderButtonShowsCheckedPlate)
{
    Window window;
    window.setBackgroundColor(Color::rgb(0xFFFFFF));
    HeaderButton* button = host(window, std::make_unique<HeaderButton>("menu"), {40, 40});
    const auto plate = [&window, button] {
        const RectF bounds = button->geometry();
        return int(window.grab().pixel(int(bounds.x + 5), int(bounds.y + 16)).g);
    };
    EXPECT_EQ(plate(), 255);
    button->setChecked(true);
    // 0x32 over white at 0.21.
    EXPECT_NEAR(plate(), 255 - 0.21 * (255 - 0x32), 1.5);
    button->setPlateAlphas({0.17, 0.26}, {0.21, 0.15});
    EXPECT_NEAR(plate(), 255 - 0.26 * (255 - 0x32), 1.5);
    button->setChecked(false);
    button->setHighlighted(true);
    EXPECT_NEAR(plate(), 255 - 0.17 * (255 - 0x32), 1.5);
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
