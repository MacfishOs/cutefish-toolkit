// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ui/geometry.h"

namespace cutefish::ui {

class View;
class Window;
struct PopupPlacement;

// FishUI's DesktopMenu, or its ComboBoxMenu as a drop-down: a popup of rows
// over the compositor's blur, driven by the pointer or the keyboard.
// Choosing a row, Escape or a press elsewhere closes it.
class Menu
{
public:
    Menu();
    ~Menu();

    Menu(const Menu&) = delete;
    Menu& operator=(const Menu&) = delete;

    // Returns the row's index. The shortcut is only shown, to the right.
    size_t addItem(std::string text, std::function<void()> action, std::string shortcut = {});
    void addSeparator();
    void setItemEnabled(size_t index, bool enabled);
    void setItemVisible(size_t index, bool visible);
    // A checked row gives every row a leading column, so the labels line up.
    void setItemChecked(size_t index, bool checked);
    // An icon from the icon theme before the text, as MenuItem's icon.name.
    void setItemIcon(size_t index, std::string icon);
    // One of FishUI's glyphs before the text, tinted like it, as MenuItem's iconName.
    void setItemGlyph(size_t index, std::string glyph);
    // The row's text in this family, as a font list previews its fonts.
    void setItemFontFamily(size_t index, std::string family);
    // As for a drop-down, never narrower than what it hangs off.
    void setMinimumWidth(double width) { m_minimumWidth = width; }
    // As ComboBoxMenu rather than DesktopMenu: at most half the screen tall
    // (240 at least) with a scroll bar while it scrolls, opened on the
    // checked row, rows found by typing their start.
    void setDropDown(bool dropDown) { m_dropDown = dropDown; }
    void clear();

    // At a point of the window, usually where the pointer is.
    void popup(Window* window, PointF position);
    // gap pixels under the view, or under a rect of it in its coordinates,
    // from its left edge; above it when there is more room there. Opened
    // from the keyboard, a drop-down lights its checked row.
    void popupFor(View* view, double gap = 3, std::optional<RectF> rect = {}, bool fromKeyboard = false);
    void close();
    bool isOpen() const { return m_window != nullptr; }
    // The popup while open, for tests.
    Window* window() const { return m_window.get(); }

    std::function<void()> onClosed;

private:
    class Rows;
    class Surface;
    friend class Rows;
    friend class Surface;

    struct Item
    {
        std::string text;
        std::string shortcut;
        std::string icon;
        std::string glyph;
        std::string fontFamily;
        std::function<void()> action;
        bool separator = false;
        bool enabled = true;
        bool visible = true;
        bool checked = false;
    };

    void open(Window* parent, const PopupPlacement& placement, bool fromKeyboard);
    void activate(size_t index);

    std::vector<Item> m_items;
    std::unique_ptr<Window> m_window;
    double m_minimumWidth = 0;
    bool m_dropDown = false;
};

} // namespace cutefish::ui
