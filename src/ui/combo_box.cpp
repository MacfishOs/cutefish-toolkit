// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/combo_box.h"

#include <algorithm>
#include <utility>

#include "ui/icon.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/menu.h"
#include "ui/painter.h"
#include "ui/theme.h"

namespace cutefish::ui {

// Three medium icons and a large spacing wide, as tall as a text field.
static constexpr double kMinimumWidth = 3 * 32 + 12;
static constexpr double kHeight = 35;
static constexpr double kChevron = 16;
static constexpr double kIcon = 22;
// Half the small spacing between the box and its list.
static constexpr double kMenuGap = 3;

ComboBox::ComboBox(std::vector<std::string> items)
    : m_label(addChild<Label>())
    , m_chevron(addChild<Icon>("chevron-down", kChevron))
    , m_icon(addChild<ThemeIcon>(std::string(), kIcon))
{
    m_label->setVerticallyCentered(true);
    m_icon->setVisible(false);
    setItems(std::move(items));
}

ComboBox::~ComboBox() = default;

void ComboBox::setItems(std::vector<std::string> items)
{
    m_items = std::move(items);
    m_current = m_items.empty() ? -1 : std::clamp(m_current, 0, int(m_items.size()) - 1);
    m_label->setText(currentText());
    updateIcon();
    requestLayout();
}

void ComboBox::setCurrentIndex(int index)
{
    if (index < 0 || index >= int(m_items.size()) || index == m_current)
        return;
    m_current = index;
    m_label->setText(currentText());
    updateIcon();
}

void ComboBox::setItemIcons(std::vector<std::string> icons)
{
    m_icons = std::move(icons);
    updateIcon();
}

void ComboBox::updateIcon()
{
    const std::string icon = m_current >= 0 && size_t(m_current) < m_icons.size() ? m_icons[size_t(m_current)]
                                                                                   : std::string();
    if (icon.empty() != !m_icon->isVisible())
        requestLayout();
    m_icon->setIconName(icon);
    m_icon->setVisible(!icon.empty());
}

bool ComboBox::isMenuOpen() const
{
    return m_menu && m_menu->isOpen();
}

std::string ComboBox::currentText() const
{
    return m_current >= 0 ? m_items[size_t(m_current)] : std::string();
}

SizeF ComboBox::sizeHint() const
{
    const Theme& theme = Theme::current();
    double widest = 0;
    for (const std::string& item : m_items) {
        Label measure(item);
        measure.setTextLayoutMode(textLayoutMode());
        widest = std::max(widest, measure.sizeHint().width);
    }
    const double padding = theme.largeSpacing;
    return {std::max(kMinimumWidth, padding + widest + theme.mediumSpacing + kChevron + padding),
            kHeight};
}

void ComboBox::layout()
{
    const Theme& theme = Theme::current();
    const double padding = theme.largeSpacing;
    const RectF bounds = localRect();
    m_chevron->setGeometry({bounds.width - padding - kChevron, 0, kChevron, bounds.height});
    // The current item's icon, beside the text.
    const double iconSpace = m_icon->isVisible() ? kIcon + theme.mediumSpacing : 0;
    m_icon->setGeometry({padding, (bounds.height - kIcon) / 2, kIcon, kIcon});
    m_label->setGeometry({padding + iconSpace, 0,
                          std::max(0.0, bounds.width - 2 * padding - kChevron - theme.mediumSpacing - iconSpace),
                          bounds.height});
    // A disabled combo box greys its text and arrow; the label
    // fades on its own, the arrow as far.
    const bool enabled = isEnabled();
    m_label->setColor(enabled ? theme.text : theme.disabledText);
    m_chevron->setColor(enabled ? theme.text : theme.disabledText);
    m_chevron->setOpacity(enabled ? 1 : 0.6);
    m_icon->setOpacity(enabled ? 1 : 0.6);
}

void ComboBox::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const bool enabled = isEnabled();
    Color color = theme.alternateBackground;
    if (enabled && m_pressed)
        color = theme.darkMode ? color.lighter(1.1) : color.darker(1.2);
    else if (enabled && m_hovered)
        color = theme.darkMode ? color.lighter(1.2) : color.darker(1.1);
    const bool open = m_menu && m_menu->isOpen();
    painter.drawRoundedRect(localRect(), {theme.smallRadius, 0, themeCorners()}, color, 1,
                            enabled && (hasFocus() || open) ? theme.highlight : color);
}

void ComboBox::enabledChanged()
{
    requestLayout();
    requestPaint();
}

void ComboBox::openMenu(bool fromKeyboard)
{
    if (isMenuOpen()) {
        m_menu->close();
        return;
    }
    if (m_items.empty() || !window())
        return;
    m_menu = std::make_unique<Menu>();
    m_menu->setDropDown(true);
    for (size_t i = 0; i < m_items.size(); ++i) {
        const size_t index = m_menu->addItem(m_items[i], [this, i] {
            setCurrentIndex(int(i));
            if (onActivated)
                onActivated(int(i));
        });
        m_menu->setItemChecked(index, int(i) == m_current);
        if (i < m_icons.size())
            m_menu->setItemIcon(index, m_icons[i]);
        if (m_fontPreview)
            m_menu->setItemFontFamily(index, m_items[i]);
    }
    m_menu->setMinimumWidth(geometry().width);
    m_menu->onClosed = [this] { requestPaint(); };
    m_menu->popupFor(this, kMenuGap, std::nullopt, fromKeyboard);
    requestPaint();
}

void ComboBox::pointerEnter()
{
    m_hovered = true;
    requestPaint();
}

void ComboBox::pointerLeave()
{
    m_hovered = false;
    requestPaint();
}

void ComboBox::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    m_pressed = true;
    requestPaint();
    openMenu(false);
}

void ComboBox::pointerRelease(const PointerEvent&)
{
    m_pressed = false;
    requestPaint();
}

bool ComboBox::keyPress(const KeyEvent& event)
{
    if (event.modifiers != NoModifier)
        return false;
    switch (event.key) {
    // These open the menu rather than stepping the current item.
    case key::Up:
    case key::Down:
    case key::F4:
    case key::Space:
    case key::Return:
    case key::KeypadEnter:
        openMenu(true);
        return true;
    default:
        return false;
    }
}

} // namespace cutefish::ui
