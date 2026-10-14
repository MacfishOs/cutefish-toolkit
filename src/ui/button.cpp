// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/button.h"

#include <algorithm>
#include <utility>

#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"

namespace cutefish::ui {

static constexpr double kMinimumWidth = 64;
static constexpr double kMinimumHeight = 32;

Button::Button(std::string text)
    : m_label(addChild<Label>(std::move(text)))
{
    m_label->setTextAlignment(Label::TextAlignment::Center);
    m_label->setVerticallyCentered(true);
}

Button::~Button() = default;

void Button::setText(std::string text)
{
    m_label->setText(std::move(text));
}

SizeF Button::sizeHint() const
{
    const Theme& theme = Theme::current();
    const SizeF text = m_label->sizeHint();
    return {std::max(kMinimumWidth, text.width + 2 * theme.largeSpacing),
            std::max(kMinimumHeight, text.height + 2 * theme.smallSpacing)};
}

void Button::layout()
{
    const double padding = Theme::current().largeSpacing;
    m_label->setGeometry(localRect().adjusted(padding, 0, -padding, 0));
}

void Button::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const bool enabled = isEnabled();
    Color color = theme.alternateBackground;
    if (enabled && m_pressed)
        color = theme.darkMode ? color.lighter(1.1) : color.darker(1.2);
    else if (enabled && m_hovered)
        color = theme.darkMode ? color.lighter(1.2) : color.darker(1.1);
    painter.fillRoundedRect(localRect(), theme.mediumRadius, color);
    if (hasVisibleFocus())
        painter.strokeRoundedRect(localRect(), theme.mediumRadius, theme.highlight.withAlpha(0.5));
}

void Button::pointerEnter()
{
    m_hovered = true;
    requestPaint();
}

void Button::pointerLeave()
{
    m_hovered = false;
    requestPaint();
}

void Button::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    m_pressed = true;
    requestPaint();
}

void Button::pointerRelease(const PointerEvent& event)
{
    if (!m_pressed)
        return;
    m_pressed = false;
    requestPaint();
    if (localRect().contains(event.position) && onClicked)
        onClicked();
}

bool Button::keyPress(const KeyEvent& event)
{
    if (event.modifiers != NoModifier)
        return false;
    if (event.key == key::Space) {
        m_pressed = true;
        requestPaint();
        return true;
    }
    if (event.key == key::Return || event.key == key::KeypadEnter) {
        if (onClicked)
            onClicked();
        return true;
    }
    return false;
}

bool Button::keyRelease(const KeyEvent& event)
{
    if (event.key != key::Space || !m_pressed)
        return false;
    m_pressed = false;
    requestPaint();
    if (onClicked)
        onClicked();
    return true;
}

} // namespace cutefish::ui
