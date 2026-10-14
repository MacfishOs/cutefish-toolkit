// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <string>

#include "ui/view.h"

namespace cutefish::ui {

class Label;

// A control with an indicator and an optional label beside it that the user
// turns on and off: the base of Switch, CheckBox and RadioButton.
class ToggleControl : public View
{
public:
    ~ToggleControl() override;

    void setText(std::string text);
    const std::string& text() const;

    bool isChecked() const { return m_checked; }
    // Changes the state without calling onToggled.
    void setChecked(bool checked);

    SizeF sizeHint() const override;
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

    // After the user changed the state.
    std::function<void(bool checked)> onToggled;

protected:
    ToggleControl(std::string text, SizeF indicator, double padding);

    bool isPressed() const { return m_pressed; }
    bool isHovered() const { return m_hovered; }
    RectF indicatorRect() const;
    // The user's action; a radio button only turns on.
    virtual void toggle();
    virtual void checkedChanged(bool animated);
    virtual void paintIndicator(Painter& painter, const RectF& rect) = 0;

    void layout() override;
    void paint(Painter& painter) override;
    void pointerEnter() override;
    void pointerLeave() override;
    void pointerPress(const PointerEvent& event) override;
    void pointerRelease(const PointerEvent& event) override;
    bool keyPress(const KeyEvent& event) override;
    bool keyRelease(const KeyEvent& event) override;

private:
    Label* m_label;
    SizeF m_indicator;
    double m_padding;
    bool m_checked = false;
    bool m_pressed = false;
    bool m_hovered = false;
};

} // namespace cutefish::ui
