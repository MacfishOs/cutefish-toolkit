// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ui/view.h"

namespace cutefish::ui {

class Icon;
class Label;
class Menu;
class ThemeIcon;

// A combo box: the current item in a field-like box with a chevron,
// opening a menu of the items with the current one checked.
class ComboBox : public View
{
public:
    explicit ComboBox(std::vector<std::string> items = {});
    ~ComboBox() override;

    void setItems(std::vector<std::string> items);
    const std::vector<std::string>& items() const { return m_items; }
    int currentIndex() const { return m_current; }
    // Does not call onActivated.
    void setCurrentIndex(int index);
    std::string currentText() const;
    // Theme icon names (or paths) beside the items, in the box and the list,
    // as an iconRole; one for each item.
    void setItemIcons(std::vector<std::string> icons);
    // Each row of the list in the font family its text names, as fontPreview.
    void setFontPreview(bool preview) { m_fontPreview = preview; }
    bool isMenuOpen() const;

    SizeF sizeHint() const override;
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

    // After the user picked an item.
    std::function<void(int index)> onActivated;

protected:
    void layout() override;
    void paint(Painter& painter) override;
    void enabledChanged() override;
    void pointerEnter() override;
    void pointerLeave() override;
    void pointerPress(const PointerEvent& event) override;
    void pointerRelease(const PointerEvent& event) override;
    bool keyPress(const KeyEvent& event) override;

private:
    void openMenu(bool fromKeyboard);
    void updateIcon();

    std::vector<std::string> m_items;
    int m_current = -1;
    Label* m_label;
    Icon* m_chevron;
    ThemeIcon* m_icon;
    std::vector<std::string> m_icons;
    bool m_fontPreview = false;
    std::unique_ptr<Menu> m_menu;
    bool m_hovered = false;
    bool m_pressed = false;
};

} // namespace cutefish::ui
