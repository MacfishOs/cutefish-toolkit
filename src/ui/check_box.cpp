// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/check_box.h"

#include <algorithm>
#include <utility>

#include "ui/icon.h"
#include "ui/painter.h"
#include "ui/quick_layout.h"
#include "ui/theme.h"

namespace cutefish::ui {

static constexpr SizeF kIndicator = {20, 20};
static constexpr double kPadding = 4;
static constexpr double kRadius = 6;
static constexpr double kCheckSize = 14;
// The focus ring, three pixels outside the indicator.
static constexpr double kRingInset = 3;

// FishUI's indicator plate: the accent colour when on, the second background
// with a faint border when off; a focus ring and, for check boxes, an inner
// light edge, both concentric through RoundedRectangle's inset.
static void paintPlate(Painter& painter, const RectF& rect, double radius, bool on, bool focused, bool innerEdge)
{
    const Theme& theme = Theme::current();
    if (focused) {
        painter.drawRoundedRect(rect.adjusted(-kRingInset, -kRingInset, kRingInset, kRingInset), {radius, -kRingInset},
                                Color{0, 0, 0, 0}, 2, theme.highlight.withAlpha(0.35));
    }
    const Color border = on ? theme.highlight.darker(1.12)
                            : theme.text.withAlpha(theme.darkMode ? 0.28 : 0.2);
    painter.drawRoundedRect(rect, {radius}, on ? theme.highlight : theme.secondBackground, 1, border);
    if (innerEdge) {
        painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), {radius, 1}, Color{0, 0, 0, 0}, 1,
                                Color::rgb(0xFFFFFF).withAlpha(on ? 0.18 : 0.06));
    }
}

CheckBox::CheckBox(std::string text)
    : ToggleControl(std::move(text), kIndicator, kPadding)
    , m_check(addChild<Icon>("check", kCheckSize))
{
    m_check->setColor(Theme::current().highlightedText);
    m_check->setVisible(false);
}

void CheckBox::layout()
{
    ToggleControl::layout();
    m_check->setGeometry(indicatorRect());
}

void CheckBox::checkedChanged(bool)
{
    m_check->setVisible(isChecked());
}

void CheckBox::paintIndicator(Painter& painter, const RectF& rect)
{
    painter.save();
    if (!isEnabled())
        painter.multiplyOpacity(0.45);
    paintPlate(painter, rect, kRadius, isChecked(), hasVisibleFocus(), true);
    painter.restore();
    m_check->setOpacity(isEnabled() ? 1 : 0.45);
}

ButtonGroup::~ButtonGroup()
{
    for (RadioButton* button : m_buttons)
        button->m_group = nullptr;
}

void ButtonGroup::add(RadioButton* button)
{
    if (std::find(m_buttons.begin(), m_buttons.end(), button) != m_buttons.end())
        return;
    if (button->m_group && button->m_group != this)
        button->m_group->remove(button);
    m_buttons.push_back(button);
    button->m_group = this;
    if (button->isChecked())
        checked(button, false);
}

void ButtonGroup::remove(RadioButton* button)
{
    std::erase(m_buttons, button);
    if (button->m_group == this)
        button->m_group = nullptr;
}

RadioButton* ButtonGroup::checkedButton() const
{
    for (RadioButton* button : m_buttons) {
        if (button->isChecked())
            return button;
    }
    return nullptr;
}

int ButtonGroup::checkedIndex() const
{
    for (size_t i = 0; i < m_buttons.size(); ++i) {
        if (m_buttons[i]->isChecked())
            return int(i);
    }
    return -1;
}

void ButtonGroup::checked(RadioButton* button, bool byUser)
{
    for (RadioButton* other : m_buttons) {
        if (other != button && other->isChecked())
            other->setChecked(false);
    }
    if (byUser && onChanged)
        onChanged(checkedIndex());
}

RadioButton::RadioButton(std::string text)
    : ToggleControl(std::move(text), kIndicator, kPadding)
{
}

RadioButton::~RadioButton()
{
    if (m_group)
        m_group->remove(this);
}

void RadioButton::setGroup(ButtonGroup* group)
{
    if (group)
        group->add(this);
    else if (m_group)
        m_group->remove(this);
}

void RadioButton::toggle()
{
    // Picking the button that is on leaves it on.
    if (isChecked())
        return;
    ToggleControl::toggle();
    if (m_group)
        m_group->checked(this, true);
}

void RadioButton::checkedChanged(bool animated)
{
    if (isChecked() && m_group && !animated)
        m_group->checked(this, false);
}

void RadioButton::paintIndicator(Painter& painter, const RectF& rect)
{
    painter.save();
    if (!isEnabled())
        painter.multiplyOpacity(0.45);
    paintPlate(painter, rect, rect.width / 2, isChecked(), hasVisibleFocus(), false);
    if (isChecked()) {
        // A plain Rectangle, anchors.centerIn the plate.
        const double dot = 8;
        painter.drawQuickRectangle({rect.x + qml::anchorsCenter(rect.width, dot),
                                    rect.y + qml::anchorsCenter(rect.height, dot), dot, dot},
                                   dot / 2, Theme::current().highlightedText);
    }
    painter.restore();
}

} // namespace cutefish::ui
