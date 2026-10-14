// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ui/geometry.h"

namespace cutefish::ui {

class View;
class Window;
struct PopupPlacement;

// FishUI's menu: a popup of rows over the compositor's blur, driven by the
// pointer or the keyboard. Choosing a row, Escape or a press elsewhere
// closes it.
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
    // As for a drop-down, never narrower than what it hangs off.
    void setMinimumWidth(double width) { m_minimumWidth = width; }
    void clear();

    // At a point of the window, usually where the pointer is.
    void popup(Window* window, PointF position);
    // Under the view, lined up with its left edge, as for a button that opens it.
    void popupFor(View* view);
    void close();
    bool isOpen() const { return m_window != nullptr; }
    // The popup while open, for tests.
    Window* window() const { return m_window.get(); }

    std::function<void()> onClosed;

private:
    class Content;
    friend class Content;

    struct Item
    {
        std::string text;
        std::string shortcut;
        std::function<void()> action;
        bool separator = false;
        bool enabled = true;
        bool visible = true;
        bool checked = false;
    };

    void open(Window* parent, const PopupPlacement& placement);
    void activate(size_t index);

    std::vector<Item> m_items;
    std::unique_ptr<Window> m_window;
    double m_minimumWidth = 0;
};

} // namespace cutefish::ui
