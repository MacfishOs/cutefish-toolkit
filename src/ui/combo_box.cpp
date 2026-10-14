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

// FishUI's: three medium icons and a large spacing wide, as tall as a text field.
static constexpr double kMinimumWidth = 3 * 32 + 12;
static constexpr double kHeight = 35;
static constexpr double kChevron = 16;

ComboBox::ComboBox(std::vector<std::string> items)
    : m_label(addChild<Label>())
    , m_chevron(addChild<Icon>("chevron-down", kChevron))
{
    m_label->setVerticallyCentered(true);
    setItems(std::move(items));
}

ComboBox::~ComboBox() = default;

void ComboBox::setItems(std::vector<std::string> items)
{
    m_items = std::move(items);
    m_current = m_items.empty() ? -1 : std::clamp(m_current, 0, int(m_items.size()) - 1);
    m_label->setText(currentText());
    requestLayout();
}

void ComboBox::setCurrentIndex(int index)
{
    if (index < 0 || index >= int(m_items.size()) || index == m_current)
        return;
    m_current = index;
    m_label->setText(currentText());
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
    m_label->setGeometry({padding, 0, std::max(0.0, bounds.width - 2 * padding - kChevron
                                                        - theme.mediumSpacing),
                          bounds.height});
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
    painter.fillRoundedRect(localRect(), theme.smallRadius, color);
    const bool open = m_menu && m_menu->isOpen();
    if (enabled && (hasFocus() || open))
        painter.strokeRoundedRect(localRect(), theme.smallRadius, theme.highlight,
                                  painter.borderWidth(1));
}

void ComboBox::openMenu()
{
    if (m_items.empty() || !window())
        return;
    m_menu = std::make_unique<Menu>();
    for (size_t i = 0; i < m_items.size(); ++i) {
        const size_t index = m_menu->addItem(m_items[i], [this, i] {
            setCurrentIndex(int(i));
            if (onActivated)
                onActivated(int(i));
        });
        m_menu->setItemChecked(index, int(i) == m_current);
    }
    m_menu->setMinimumWidth(geometry().width);
    m_menu->onClosed = [this] { requestPaint(); };
    m_menu->popupFor(this);
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
    openMenu();
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
    case key::Space:
    case key::Return:
    case key::KeypadEnter:
        openMenu();
        return true;
    case key::Up:
        if (m_current > 0) {
            setCurrentIndex(m_current - 1);
            if (onActivated)
                onActivated(m_current);
        }
        return true;
    case key::Down:
        if (m_current + 1 < int(m_items.size())) {
            setCurrentIndex(m_current + 1);
            if (onActivated)
                onActivated(m_current);
        }
        return true;
    default:
        return false;
    }
}

} // namespace cutefish::ui
