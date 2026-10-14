// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/menu.h"

#include <algorithm>
#include <cmath>

#include "base/event_loop.h"
#include "ui/application.h"
#include "ui/icon.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/view.h"
#include "ui/window.h"

namespace cutefish::ui {

// FishUI's MenuSurface and MenuItem metrics.
static constexpr double kRowHeight = 30;
static constexpr double kSeparatorHeight = 5;
static constexpr double kMinimumRowWidth = 146;
static constexpr double kContentMargin = 3;
static constexpr double kVerticalContentMargin = 5;
// The highlight inside a row.
static constexpr double kHighlightInsetX = 3;
static constexpr double kHighlightInsetY = 1;
static constexpr double kRowPadding = 8;
static constexpr double kLabelPadding = 4;
static constexpr double kShortcutSpacing = 24;
static constexpr double kCheckSize = 16;
static constexpr double kCheckSpacing = 6;

class Menu::Content : public View
{
public:
    explicit Content(Menu* menu)
        : m_menu(menu)
    {
        for (const Item& item : menu->m_items) {
            if (item.visible && item.checked)
                m_checkColumn = true;
        }
        for (size_t i = 0; i < menu->m_items.size(); ++i) {
            const Item& item = menu->m_items[i];
            if (!item.visible)
                continue;
            Row row{i, nullptr, nullptr, nullptr};
            if (!item.separator) {
                row.label = addChild<Label>(item.text);
                row.label->setVerticallyCentered(true);
                row.label->setColor(color(i, false));
                if (item.checked) {
                    row.check = addChild<Icon>("check", kCheckSize);
                    row.check->setColor(color(i, false));
                }
                if (!item.shortcut.empty()) {
                    row.shortcut = addChild<Label>(item.shortcut);
                    row.shortcut->setVerticallyCentered(true);
                    row.shortcut->setColor(Theme::current().disabledText);
                }
            }
            m_rows.push_back(row);
        }
        // A drop-down opens on its current item, ready for the arrows.
        for (size_t i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].label && menu->m_items[m_rows[i].index].checked) {
                m_keyboard = true;
                setCurrent(int(i));
                break;
            }
        }
    }

    SizeF sizeHint() const override
    {
        double width = kMinimumRowWidth;
        double height = 2 * kVerticalContentMargin;
        for (const Row& row : m_rows) {
            if (!row.label) {
                height += kSeparatorHeight;
                continue;
            }
            double rowWidth = 2 * kRowPadding + labelPadding() + row.label->sizeHint().width;
            if (row.shortcut)
                rowWidth += kShortcutSpacing + row.shortcut->sizeHint().width;
            width = std::max(width, std::ceil(rowWidth));
            height += kRowHeight;
        }
        return {std::max(width + 2 * kContentMargin, m_menu->m_minimumWidth), height};
    }

    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

protected:
    void layout() override
    {
        double y = kVerticalContentMargin;
        const double width = geometry().width - 2 * kContentMargin;
        for (Row& row : m_rows) {
            if (!row.label) {
                row.rect = {kContentMargin, y, width, kSeparatorHeight};
                y += kSeparatorHeight;
                continue;
            }
            row.rect = {kContentMargin, y, width, kRowHeight};
            const RectF text = row.rect.adjusted(kRowPadding + labelPadding(), 0, -kRowPadding, 0);
            if (row.check)
                row.check->setGeometry({row.rect.x + kRowPadding, row.rect.y, kCheckSize, kRowHeight});
            if (row.shortcut) {
                const double shortcutWidth = row.shortcut->sizeHint().width;
                row.shortcut->setGeometry({text.right() - shortcutWidth, text.y, shortcutWidth,
                                           text.height});
            }
            row.label->setGeometry(text);
            y += kRowHeight;
        }
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        // Over the compositor's blur; the window radius is the compositor's too.
        painter.fillRoundedRect(localRect(), theme.windowRadius,
                                theme.secondBackground.withAlpha(0.8));
        for (size_t i = 0; i < m_rows.size(); ++i) {
            const Row& row = m_rows[i];
            if (!row.label) {
                const Color line = theme.darkMode ? Color::rgb(0xFFFFFF).withAlpha(0.14)
                                                  : Color::rgb(0x000000).withAlpha(0.1);
                painter.drawHorizontalLine(row.rect.x + theme.largeSpacing,
                                           row.rect.y + std::floor(kSeparatorHeight / 2),
                                           row.rect.width - 2 * theme.largeSpacing, line);
                continue;
            }
            if (int(i) == m_current) {
                // Concentric with the menu: the window radius less the insets.
                const RectF highlight = row.rect.adjusted(kHighlightInsetX, kHighlightInsetY,
                                                          -kHighlightInsetX, -kHighlightInsetY);
                const Color color = theme.highlight.withAlpha(theme.darkMode ? 0.82 : 0.9);
                painter.fillRoundedRect(highlight, theme.smallRadius, color);
            }
        }
    }

    void pointerMove(const PointerEvent& event) override { hover(event.position); }
    void pointerEnter() override {}

    void pointerLeave() override
    {
        if (!m_keyboard)
            setCurrent(-1);
    }

    void pointerPress(const PointerEvent& event) override { hover(event.position); }

    void pointerRelease(const PointerEvent& event) override
    {
        // Choosing is on release, over the row the pointer is on then.
        const int row = rowAt(event.position);
        if (row >= 0)
            m_menu->activate(m_rows[size_t(row)].index);
    }

    bool keyPress(const KeyEvent& event) override
    {
        switch (event.key) {
        case key::Up:
            step(-1);
            return true;
        case key::Down:
            step(1);
            return true;
        case key::Home:
            m_current = -1;
            step(1);
            return true;
        case key::End:
            m_current = int(m_rows.size());
            step(-1);
            return true;
        case key::Return:
        case key::KeypadEnter:
        case key::Space:
            if (m_current >= 0)
                m_menu->activate(m_rows[size_t(m_current)].index);
            return true;
        case key::Escape:
            m_menu->close();
            return true;
        default:
            return true;
        }
    }

private:
    struct Row
    {
        size_t index;
        Label* label;
        Label* shortcut;
        Icon* check;
        RectF rect;
    };

    bool selectable(int row) const
    {
        return row >= 0 && row < int(m_rows.size()) && m_rows[size_t(row)].label
                && m_menu->m_items[m_rows[size_t(row)].index].enabled;
    }

    int rowAt(PointF point) const
    {
        for (size_t i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].rect.contains(point))
                return selectable(int(i)) ? int(i) : -1;
        }
        return -1;
    }

    void hover(PointF point)
    {
        m_keyboard = false;
        setCurrent(rowAt(point));
    }

    void step(int direction)
    {
        m_keyboard = true;
        int row = m_current;
        for (size_t tries = 0; tries < m_rows.size(); ++tries) {
            row += direction;
            if (row < 0)
                row = int(m_rows.size()) - 1;
            else if (row >= int(m_rows.size()))
                row = 0;
            if (selectable(row)) {
                setCurrent(row);
                return;
            }
        }
    }

    void setCurrent(int row)
    {
        if (row == m_current)
            return;
        m_current = row;
        for (size_t i = 0; i < m_rows.size(); ++i) {
            const std::optional<Color> rowColor = color(m_rows[i].index, int(i) == m_current);
            if (m_rows[i].label)
                m_rows[i].label->setColor(rowColor);
            if (m_rows[i].check)
                m_rows[i].check->setColor(rowColor);
        }
        requestPaint();
    }

    double labelPadding() const
    {
        return m_checkColumn ? kCheckSize + kCheckSpacing : kLabelPadding;
    }

    std::optional<Color> color(size_t item, bool active) const
    {
        const Theme& theme = Theme::current();
        if (!m_menu->m_items[item].enabled)
            return theme.text.withAlpha(0.4);
        if (active)
            return theme.highlightedText;
        return std::nullopt;
    }

    Menu* m_menu;
    std::vector<Row> m_rows;
    bool m_checkColumn = false;
    int m_current = -1;
    // The highlight follows the keyboard until the pointer moves.
    bool m_keyboard = false;
};

Menu::Menu() = default;

Menu::~Menu()
{
    m_window.reset();
}

size_t Menu::addItem(std::string text, std::function<void()> action, std::string shortcut)
{
    Item item;
    item.text = std::move(text);
    item.shortcut = std::move(shortcut);
    item.action = std::move(action);
    m_items.push_back(std::move(item));
    return m_items.size() - 1;
}

void Menu::addSeparator()
{
    Item item;
    item.separator = true;
    m_items.push_back(std::move(item));
}

void Menu::setItemEnabled(size_t index, bool enabled)
{
    if (index < m_items.size())
        m_items[index].enabled = enabled;
}

void Menu::setItemVisible(size_t index, bool visible)
{
    if (index < m_items.size())
        m_items[index].visible = visible;
}

void Menu::setItemChecked(size_t index, bool checked)
{
    if (index < m_items.size())
        m_items[index].checked = checked;
}

void Menu::clear()
{
    m_items.clear();
}

void Menu::popup(Window* window, PointF position)
{
    PopupPlacement placement;
    placement.anchorRect = {position.x, position.y, 1, 1};
    placement.anchor = PopupAnchor::TopLeft;
    placement.gravity = PopupAnchor::BottomRight;
    open(window, placement);
}

void Menu::popupFor(View* view)
{
    Window* window = view->window();
    if (!window)
        return;
    const PointF topLeft = view->mapToWindow({});
    PopupPlacement placement;
    placement.anchorRect = {topLeft.x, topLeft.y, view->geometry().width, view->geometry().height};
    placement.anchor = PopupAnchor::BottomLeft;
    placement.gravity = PopupAnchor::BottomRight;
    placement.offset = {0, Theme::current().smallSpacing};
    open(window, placement);
}

void Menu::open(Window* parent, const PopupPlacement& placement)
{
    close();
    m_window = std::make_unique<Window>(parent, placement, true);
    auto content = std::make_unique<Content>(this);
    Content* view = content.get();
    m_window->resize(content->sizeHint());
    m_window->setBackgroundColor(Color::rgb(0x000000).withAlpha(0));
    m_window->setContentView(std::move(content));
    const SizeF size = m_window->size();
    m_window->setBackgroundBlur({0, 0, size.width, size.height}, Theme::current().windowRadius);
    m_window->onCloseRequested = [this] { close(); };
    view->setFocus();
    m_window->show();
}

void Menu::close()
{
    if (!m_window)
        return;
    // A row closes the menu from inside the popup's own event handling; the
    // window goes once that is over.
    std::shared_ptr<Window> window(std::move(m_window));
    Application::instance()->loop().post([window] {});
    if (onClosed)
        onClosed();
}

void Menu::activate(size_t index)
{
    if (index >= m_items.size() || !m_items[index].enabled)
        return;
    const std::function<void()> action = m_items[index].action;
    close();
    if (action)
        action();
}

} // namespace cutefish::ui
