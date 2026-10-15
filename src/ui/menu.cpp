// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/menu.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "base/event_loop.h"
#include "ui/application.h"
#include "ui/icon.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/scroll_view.h"
#include "ui/theme.h"
#include "ui/view.h"
#include "ui/window.h"

namespace cutefish::ui {

// FishUI's MenuItem, MenuSeparator and MenuSurface metrics.
static constexpr double kRowHeight = 30;
static constexpr double kRowSpacing = 1;
static constexpr double kSeparatorHeight = 5;
static constexpr double kMinimumRowWidth = 146;
static constexpr double kContentMargin = 3;
static constexpr double kVerticalContentMargin = 5;
// The highlight inside a row.
static constexpr double kHighlightInsetX = 3;
static constexpr double kHighlightInsetY = 1;
static constexpr double kRowPadding = 8;
static constexpr double kLabelPadding = 4;
static constexpr double kItemSpacing = 9;
static constexpr double kShortcutSpacing = 24;
static constexpr double kCheckSize = 12;
static constexpr double kCheckSpacing = 6;
static constexpr double kIconSize = 22;
static constexpr double kGlyphSize = 16;
// MenuPopupWindow keeps this far from the screen's edges.
static constexpr double kScreenMargin = 6;
static constexpr double kDropDownHeight = 240;
// DesktopMenu's scroll indicator.
static constexpr double kIndicatorWidth = 3;
static constexpr double kIndicatorMinimumLength = 24;
// Letters typed within this long of each other make up one search.
static constexpr std::chrono::milliseconds kSearchTimeout{1000};

static std::string lowered(const std::string& text)
{
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return char(std::tolower(c));
    });
    return result;
}

// The rows, in a ScrollView of the surface: MenuItems and MenuSeparators in
// a column with one pixel between them.
class Menu::Rows : public View
{
public:
    Rows(Menu* menu, bool fromKeyboard)
        : m_menu(menu)
        , m_keyboard(fromKeyboard)
    {
        for (const Item& item : menu->m_items) {
            if (item.visible && item.checked)
                m_checkColumn = true;
        }
        for (size_t i = 0; i < menu->m_items.size(); ++i) {
            const Item& item = menu->m_items[i];
            if (!item.visible)
                continue;
            Row row;
            row.index = i;
            if (!item.separator) {
                row.label = addChild<Label>(item.text);
                row.label->setVerticallyCentered(true);
                row.label->setFontFamily(item.fontFamily);
                if (item.checked)
                    row.check = addChild<Icon>("check", kCheckSize);
                if (!item.glyph.empty())
                    row.glyph = addChild<Icon>(item.glyph, kGlyphSize);
                else if (!item.icon.empty())
                    row.icon = addChild<ThemeIcon>(item.icon, kIconSize);
                if (!item.shortcut.empty()) {
                    row.shortcut = addChild<Label>(item.shortcut);
                    row.shortcut->setVerticallyCentered(true);
                }
            }
            m_rows.push_back(row);
        }
        // Opened from the keyboard a drop-down lights its chosen row, so
        // Return keeps it and the arrows move on from it.
        if (fromKeyboard && menu->m_dropDown)
            m_current = checkedRow();
        updateColors();
    }

    ~Rows() override
    {
        if (m_searchTimer)
            Application::instance()->loop().stopTimer(m_searchTimer);
    }

    void setScrollView(ScrollView* view) { m_scrollView = view; }

    SizeF sizeHint() const override
    {
        double width = 0;
        double height = 0;
        for (size_t i = 0; i < m_rows.size(); ++i) {
            const Row& row = m_rows[i];
            height += (i > 0 ? kRowSpacing : 0) + (row.label ? kRowHeight : kSeparatorHeight);
            if (row.label)
                width = std::max(width, rowWidth(row));
        }
        return {width, height};
    }

    bool acceptsPointer() const override { return true; }

    int checkedRow() const
    {
        for (size_t i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].label && m_menu->m_items[m_rows[i].index].checked)
                return int(i);
        }
        return -1;
    }

    // ListView.positionViewAtIndex with ListView.Center, once laid out.
    void centreOnOpen(int row) { m_centreRow = row; }

    void pointerMoved(PointF windowPoint)
    {
        m_pointer = windowPoint;
        m_pointerInside = true;
        const int row = rowAt(mapFromWindow(windowPoint));
        // After a key, motion elsewhere leaves the highlight alone until the
        // pointer comes back over the list.
        if (m_keyboard && row < 0 && m_menu->m_dropDown)
            return;
        m_keyboard = false;
        setCurrent(row);
    }

    void pointerLeft()
    {
        m_pointerInside = false;
        if (!m_keyboard)
            setCurrent(-1);
    }

    // The view moved under a still pointer.
    void scrolled()
    {
        if (m_pointerInside && !m_keyboard)
            setCurrent(rowAt(mapFromWindow(m_pointer)));
    }

    bool key(const KeyEvent& event)
    {
        return m_menu->m_dropDown ? dropDownKey(event) : desktopKey(event);
    }

protected:
    void layout() override
    {
        double y = 0;
        const double width = geometry().width;
        for (Row& row : m_rows) {
            if (!row.label) {
                row.rect = {0, y, width, kSeparatorHeight};
                y += kSeparatorHeight + kRowSpacing;
                continue;
            }
            row.rect = {0, y, width, kRowHeight};
            const double lead = labelPadding();
            if (row.check) {
                row.check->setGeometry({row.rect.x + kRowPadding, y + std::round((kRowHeight - kCheckSize) / 2),
                                        kCheckSize, kCheckSize});
            }
            if (row.glyph) {
                row.glyph->setGeometry({row.rect.x + kRowPadding + lead, y + std::round((kRowHeight - kGlyphSize) / 2),
                                        kGlyphSize, kGlyphSize});
            }
            if (row.icon) {
                row.icon->setGeometry({row.rect.x + kRowPadding + lead, y + std::round((kRowHeight - kIconSize) / 2),
                                       kIconSize, kIconSize});
            }
            double right = row.rect.right() - kRowPadding;
            if (row.shortcut) {
                const double shortcutWidth = row.shortcut->sizeHint().width;
                row.shortcut->setGeometry({right - shortcutWidth, y, shortcutWidth, kRowHeight});
                right -= shortcutWidth + kShortcutSpacing;
            }
            const double left = row.rect.x + kRowPadding + lead + glyphPadding(row);
            row.label->setGeometry({left, y, std::max(0.0, right - left), kRowHeight});
            y += kRowHeight + kRowSpacing;
        }

        if (m_centreRow >= 0 && m_scrollView) {
            const RectF& rect = m_rows[size_t(m_centreRow)].rect;
            m_centreRow = -1;
            const double viewport = m_scrollView->geometry().height;
            m_scrollView->setScrollPosition({0, rect.y + rect.height / 2 - viewport / 2});
        }
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        for (size_t i = 0; i < m_rows.size(); ++i) {
            const Row& row = m_rows[i];
            if (!row.label) {
                // One device pixel, centred, kept on the pixel grid.
                const Color line = theme.darkMode ? Color::rgb(0xFFFFFF).withAlpha(0.14)
                                                  : Color::rgb(0x000000).withAlpha(0.1);
                const double height = 1 / painter.scale();
                painter.drawRoundedRect({row.rect.x + theme.largeSpacing, row.rect.y + (kSeparatorHeight - height) / 2,
                                         row.rect.width - 2 * theme.largeSpacing, height},
                                        {0}, line);
                continue;
            }
            if (int(i) == m_current && m_menu->m_items[row.index].enabled) {
                const RectF highlight = row.rect.adjusted(kHighlightInsetX, kHighlightInsetY,
                                                          -kHighlightInsetX, -kHighlightInsetY);
                const Color color = theme.highlight.withAlpha(theme.darkMode ? 0.82 : 0.9);
                painter.drawRoundedRect(highlight, {theme.smallRadius, 0, themeCorners()}, color);
            }
        }
    }

    void pointerMove(const PointerEvent& event) override { pointerMoved(mapToWindow(event.position)); }
    void pointerPress(const PointerEvent& event) override { pointerMoved(mapToWindow(event.position)); }
    void pointerLeave() override { pointerLeft(); }

    void pointerRelease(const PointerEvent& event) override
    {
        // Choosing is on release, over the row the pointer is on then.
        const int row = rowAt(event.position);
        if (row >= 0)
            m_menu->activate(m_rows[size_t(row)].index);
    }

private:
    struct Row
    {
        size_t index = 0;
        Label* label = nullptr;
        Label* shortcut = nullptr;
        Icon* check = nullptr;
        Icon* glyph = nullptr;
        ThemeIcon* icon = nullptr;
        RectF rect;
    };

    double labelPadding() const { return m_checkColumn ? kCheckSize + kCheckSpacing : kLabelPadding; }

    double glyphPadding(const Row& row) const
    {
        if (row.glyph)
            return kGlyphSize + kItemSpacing;
        if (row.icon)
            return kIconSize + kItemSpacing;
        return 0;
    }

    // MenuItem's implicit width; a drop-down measures its rows in the menu's
    // font, as ComboBoxMenu's probe row does.
    double rowWidth(const Row& row) const
    {
        double text = row.label->sizeHint().width;
        const Item& item = m_menu->m_items[row.index];
        if (!item.fontFamily.empty()) {
            Label probe(item.text);
            text = probe.sizeHint().width;
        }
        double width = labelPadding() + glyphPadding(row) + text;
        if (row.shortcut)
            width += row.shortcut->sizeHint().width + kShortcutSpacing;
        return std::max(kMinimumRowWidth, std::ceil(width) + 2 * kRowPadding);
    }

    bool selectable(int row) const
    {
        return row >= 0 && row < int(m_rows.size()) && m_rows[size_t(row)].label
                && m_menu->m_items[m_rows[size_t(row)].index].enabled;
    }

    int rowAt(PointF point) const
    {
        if (!localRect().contains(point))
            return -1;
        for (size_t i = 0; i < m_rows.size(); ++i) {
            if (point.y >= m_rows[i].rect.y && point.y < m_rows[i].rect.bottom())
                return m_rows[i].label ? int(i) : -1;
        }
        return -1;
    }

    // DesktopMenu's keys: the arrows wrap round the enabled rows.
    bool desktopKey(const KeyEvent& event)
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
            m_current = -1;
            step(-1);
            return true;
        case key::Return:
        case key::KeypadEnter:
            if (selectable(m_current))
                m_menu->activate(m_rows[size_t(m_current)].index);
            else if (m_current < 0)
                m_menu->close();
            return true;
        case key::Space:
            if (selectable(m_current))
                m_menu->activate(m_rows[size_t(m_current)].index);
            return true;
        case key::Escape:
            m_menu->close();
            return true;
        default:
            return true;
        }
    }

    // ComboBoxMenu's keys: the arrows stop at the ends, and typing finds rows.
    bool dropDownKey(const KeyEvent& event)
    {
        switch (event.key) {
        case key::Down:
            moveHighlight(1);
            return true;
        case key::Up:
            moveHighlight(-1);
            return true;
        case key::PageDown:
            moveHighlight(pageStep());
            return true;
        case key::PageUp:
            moveHighlight(-pageStep());
            return true;
        case key::Home:
            setHighlight(0);
            return true;
        case key::End:
            setHighlight(int(m_rows.size()) - 1);
            return true;
        case key::Return:
        case key::KeypadEnter:
            activateHighlighted();
            return true;
        case key::Space:
            // Part of a name being typed, such as "Noto Sans".
            if (!m_search.empty())
                search(" ");
            else
                activateHighlighted();
            return true;
        case key::Escape:
        case key::Tab:
            m_menu->close();
            return true;
        default:
            if (!event.text.empty() && static_cast<unsigned char>(event.text[0]) >= 0x20
                && !(event.modifiers & (ControlModifier | AltModifier | SuperModifier)))
                search(event.text);
            return true;
        }
    }

    void step(int direction)
    {
        m_keyboard = true;
        int row = m_current;
        if (row < 0)
            row = direction > 0 ? -1 : int(m_rows.size());
        for (size_t tries = 0; tries < m_rows.size(); ++tries) {
            row = (row + direction + int(m_rows.size()) * 2) % int(m_rows.size());
            if (selectable(row)) {
                setCurrent(row);
                ensureVisible(row);
                return;
            }
        }
    }

    void setHighlight(int row)
    {
        if (m_rows.empty())
            return;
        row = std::clamp(row, 0, int(m_rows.size()) - 1);
        m_keyboard = true;
        setCurrent(row);
        ensureVisible(row);
    }

    // Keys move on from the lit row, or from the chosen one while the
    // pointer has lit none.
    void moveHighlight(int step)
    {
        const int from = m_current >= 0 ? m_current : checkedRow();
        if (from < 0)
            setHighlight(step > 0 ? 0 : int(m_rows.size()) - 1);
        else
            setHighlight(from + step);
    }

    int pageStep() const
    {
        const double viewport = m_scrollView ? m_scrollView->geometry().height : 0;
        return std::max(1, int(std::floor(viewport / (kRowHeight + kRowSpacing))) - 1);
    }

    void activateHighlighted()
    {
        if (selectable(m_current))
            m_menu->activate(m_rows[size_t(m_current)].index);
        else
            m_menu->close();
    }

    // Typing lights the next row that starts with what was typed; the same
    // letter again steps through the rows starting with it.
    void search(const std::string& typed)
    {
        m_search += lowered(typed);
        auto& loop = Application::instance()->loop();
        if (m_searchTimer)
            loop.stopTimer(m_searchTimer);
        m_searchTimer = loop.startTimer(kSearchTimeout, false, [this] {
            m_searchTimer = 0;
            m_search.clear();
        });

        const bool repeated = std::all_of(m_search.begin(), m_search.end(),
                                          [this](char c) { return c == m_search.front(); });
        const std::string prefix = repeated ? m_search.substr(0, 1) : m_search;
        const int start = m_current >= 0 ? m_current : checkedRow();
        const int first = start < 0 ? 0 : start + (repeated ? 1 : 0);
        const int count = int(m_rows.size());
        for (int i = 0; i < count; ++i) {
            const int row = (first + i) % count;
            if (m_rows[size_t(row)].label && lowered(m_menu->m_items[m_rows[size_t(row)].index].text).starts_with(prefix)) {
                setHighlight(row);
                return;
            }
        }
    }

    void ensureVisible(int row)
    {
        if (m_scrollView && row >= 0)
            m_scrollView->ensureVisible(m_rows[size_t(row)].rect);
    }

    void setCurrent(int row)
    {
        if (row == m_current)
            return;
        m_current = row;
        updateColors();
        requestPaint();
    }

    void updateColors()
    {
        const Theme& theme = Theme::current();
        for (size_t i = 0; i < m_rows.size(); ++i) {
            const Row& row = m_rows[i];
            if (!row.label)
                continue;
            const bool enabled = m_menu->m_items[row.index].enabled;
            const bool active = enabled && int(i) == m_current;
            const Color disabled = theme.text.withAlpha(0.4);
            const std::optional<Color> text = !enabled ? std::optional<Color>(disabled)
                                                       : active ? std::optional<Color>(theme.highlightedText)
                                                                : std::nullopt;
            row.label->setColor(text);
            if (row.check)
                row.check->setColor(text);
            if (row.glyph)
                row.glyph->setColor(text);
            if (row.shortcut)
                row.shortcut->setColor(!enabled ? disabled : active ? theme.highlightedText : theme.text.withAlpha(0.5));
        }
    }

    Menu* m_menu;
    ScrollView* m_scrollView = nullptr;
    std::vector<Row> m_rows;
    bool m_checkColumn = false;
    int m_current = -1;
    // The highlight follows the keyboard until the pointer moves over a row.
    bool m_keyboard = false;
    bool m_pointerInside = false;
    PointF m_pointer;
    int m_centreRow = -1;
    std::string m_search;
    int m_searchTimer = 0;
};

// DesktopMenu's scroll indicator, drawn over the rows of a menu that scrolls.
class ScrollIndicator : public View
{
public:
    explicit ScrollIndicator(const ScrollView* view)
        : m_view(view)
    {
    }

protected:
    void paint(Painter& painter) override
    {
        const RectF view = m_view->geometry();
        const double content = m_view->contentSize().height;
        if (content <= view.height)
            return;
        const Theme& theme = Theme::current();
        const double length = std::max(kIndicatorMinimumLength, view.height * view.height / content);
        const RectF bar = {view.right() - kIndicatorWidth,
                           view.y + view.height * (m_view->scrollPosition().y / content), kIndicatorWidth, length};
        painter.drawQuickRectangle(bar, kIndicatorWidth / 2,
                                   theme.darkMode ? Color::rgb(0xFFFFFF).withAlpha(0.35)
                                                  : Color::rgb(0x000000).withAlpha(0.25));
    }

private:
    const ScrollView* m_view;
};

// The popup's content: MenuSurface's background and the scrolling rows,
// with ComboBoxMenu's scroll bar or DesktopMenu's indicator.
class Menu::Surface : public View
{
public:
    Surface(Menu* menu, bool fromKeyboard)
        : m_menu(menu)
        , m_scroll(addChild<ScrollView>())
    {
        m_rows = m_scroll->setContent<Rows>(menu, fromKeyboard);
        m_rows->setScrollView(m_scroll);
        m_scroll->setElastic(false);
        m_scroll->setVerticalBarPolicy(menu->m_dropDown ? ScrollView::BarPolicy::AlwaysOn
                                                        : ScrollView::BarPolicy::AlwaysOff);
        if (!menu->m_dropDown)
            m_indicator = addChild<ScrollIndicator>(m_scroll);
        m_scroll->onScrolled = [this] {
            m_rows->scrolled();
            if (m_indicator)
                m_indicator->requestPaint();
        };
        // The row under the pointer is lit whether or not a button is held.
        setTracksHover(true);
    }

    SizeF sizeHint() const override
    {
        const SizeF rows = m_rows->sizeHint();
        return {std::max(m_menu->m_minimumWidth, rows.width + 2 * kContentMargin),
                rows.height + 2 * kVerticalContentMargin};
    }

    Rows* rows() const { return m_rows; }

    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

protected:
    void layout() override
    {
        m_scroll->setGeometry(localRect().adjusted(kContentMargin, kVerticalContentMargin, -kContentMargin,
                                                   -kVerticalContentMargin));
        if (m_indicator)
            m_indicator->setGeometry(localRect());
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        // Over the compositor's blur; the window radius is the compositor's too.
        painter.fillRoundedRect(localRect(), theme.windowRadius, theme.secondBackground.withAlpha(0.8));
    }

    void pointerMove(const PointerEvent& event) override { m_rows->pointerMoved(mapToWindow(event.position)); }
    void hoverMove(const PointerEvent& event) override { m_rows->pointerMoved(mapToWindow(event.position)); }
    void hoverChanged() override
    {
        if (!isHovered())
            m_rows->pointerLeft();
    }
    bool keyPress(const KeyEvent& event) override { return m_rows->key(event); }

private:
    Menu* m_menu;
    ScrollView* m_scroll;
    Rows* m_rows = nullptr;
    ScrollIndicator* m_indicator = nullptr;
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

void Menu::setItemIcon(size_t index, std::string icon)
{
    if (index < m_items.size())
        m_items[index].icon = std::move(icon);
}

void Menu::setItemGlyph(size_t index, std::string glyph)
{
    if (index < m_items.size())
        m_items[index].glyph = std::move(glyph);
}

void Menu::setItemFontFamily(size_t index, std::string family)
{
    if (index < m_items.size())
        m_items[index].fontFamily = std::move(family);
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
    open(window, placement, false);
}

void Menu::popupFor(View* view, double gap, std::optional<RectF> rect, bool fromKeyboard)
{
    Window* window = view->window();
    if (!window)
        return;
    const RectF local = rect.value_or(view->localRect());
    const PointF topLeft = view->mapToWindow({local.x, local.y});
    // MenuPopupWindow's anchor: the item's rect on whole pixels, gap above
    // and below it; the compositor flips it above when it does not fit.
    const double x = std::round(topLeft.x);
    const double y = std::round(topLeft.y);
    PopupPlacement placement;
    placement.anchorRect = {x, y - gap, std::ceil(local.width), std::ceil(local.height) + 2 * gap};
    placement.anchor = PopupAnchor::BottomLeft;
    placement.gravity = PopupAnchor::BottomRight;
    placement.flipX = false;
    open(window, placement, fromKeyboard);
}

void Menu::open(Window* parent, const PopupPlacement& placement, bool fromKeyboard)
{
    close();
    m_window = std::make_unique<Window>(parent, placement, true);
    auto content = std::make_unique<Surface>(this, fromKeyboard);
    Surface* surface = content.get();
    SizeF size = content->sizeHint();
    size.width = std::ceil(size.width);
    size.height = std::ceil(size.height);

    // A drop-down is at most half the screen tall, and a desktop menu the
    // screen, so long lists scroll.
    const SizeF screen = parent->screenSize();
    if (screen.height > 0) {
        const double available = screen.height - 2 * kScreenMargin;
        const double maximum = m_dropDown ? std::max(std::min(available, kDropDownHeight), std::round(available * 0.5))
                                          : available;
        size.height = std::min(size.height, maximum);
    }
    if (m_dropDown)
        surface->rows()->centreOnOpen(surface->rows()->checkedRow());

    m_window->resize(size);
    m_window->setBackgroundColor(Color::rgb(0x000000).withAlpha(0));
    m_window->setContentView(std::move(content));
    m_window->setBackgroundBlur({0, 0, size.width, size.height}, Theme::current().windowRadius);
    m_window->onCloseRequested = [this] { close(); };
    surface->setFocus(fromKeyboard ? FocusReason::Keyboard : FocusReason::Other);
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
