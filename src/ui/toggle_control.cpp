// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/toggle_control.h"

#include <algorithm>
#include <utility>

#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"

namespace cutefish::ui {

ToggleControl::ToggleControl(std::string text, SizeF indicator, double padding)
    : m_label(addChild<Label>(std::move(text)))
    , m_indicator(indicator)
    , m_padding(padding)
{
    m_label->setVerticallyCentered(true);
}

ToggleControl::~ToggleControl() = default;

void ToggleControl::setText(std::string text)
{
    m_label->setText(std::move(text));
}

const std::string& ToggleControl::text() const
{
    return m_label->text();
}

void ToggleControl::setChecked(bool checked)
{
    if (checked == m_checked)
        return;
    m_checked = checked;
    checkedChanged(false);
    requestPaint();
}

SizeF ToggleControl::sizeHint() const
{
    if (text().empty())
        return {m_indicator.width + 2 * m_padding, m_indicator.height};
    const SizeF label = m_label->sizeHint();
    const double spacing = Theme::current().mediumSpacing;
    return {m_padding + m_indicator.width + spacing + label.width + m_padding,
            std::max(m_indicator.height, label.height) + 2 * m_padding};
}

RectF ToggleControl::indicatorRect() const
{
    const RectF bounds = localRect();
    const double x = text().empty() ? (bounds.width - m_indicator.width) / 2 : m_padding;
    return {x, (bounds.height - m_indicator.height) / 2, m_indicator.width, m_indicator.height};
}

void ToggleControl::layout()
{
    const RectF indicator = indicatorRect();
    const double left = indicator.right() + Theme::current().mediumSpacing;
    m_label->setVisible(!text().empty());
    m_label->setGeometry({left, 0, std::max(0.0, geometry().width - left - m_padding),
                          geometry().height});
}

void ToggleControl::paint(Painter& painter)
{
    paintIndicator(painter, indicatorRect());
}

void ToggleControl::toggle()
{
    m_checked = !m_checked;
    checkedChanged(true);
    requestPaint();
    if (onToggled)
        onToggled(m_checked);
}

void ToggleControl::checkedChanged(bool)
{
}

void ToggleControl::pointerEnter()
{
    m_hovered = true;
    requestPaint();
}

void ToggleControl::pointerLeave()
{
    m_hovered = false;
    requestPaint();
}

void ToggleControl::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    m_pressed = true;
    requestPaint();
}

void ToggleControl::pointerRelease(const PointerEvent& event)
{
    if (!m_pressed)
        return;
    m_pressed = false;
    requestPaint();
    if (localRect().contains(event.position))
        toggle();
}

bool ToggleControl::keyPress(const KeyEvent& event)
{
    if (event.key != key::Space || event.modifiers != NoModifier)
        return false;
    m_pressed = true;
    requestPaint();
    return true;
}

bool ToggleControl::keyRelease(const KeyEvent& event)
{
    if (event.key != key::Space || !m_pressed)
        return false;
    m_pressed = false;
    toggle();
    return true;
}

} // namespace cutefish::ui
