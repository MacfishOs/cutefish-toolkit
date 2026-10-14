// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>

#include "ui/view.h"

namespace cutefish::ui {

// FishUI's horizontal slider: a thin track filled in the accent colour up to
// a round white handle with a soft shadow. A stepped slider snaps to whole
// steps and marks them under the track.
class Slider : public View
{
public:
    Slider();

    void setRange(double from, double to);
    // Zero for a continuous slider.
    void setStepSize(double step);
    double value() const { return m_value; }
    // Clamped to the range; does not call onMoved.
    void setValue(double value);

    SizeF sizeHint() const override;
    RectF visualRect() const override;
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

    // While the user moves the handle.
    std::function<void(double value)> onMoved;

protected:
    void paint(Painter& painter) override;
    void pointerPress(const PointerEvent& event) override;
    void pointerMove(const PointerEvent& event) override;
    void pointerRelease(const PointerEvent& event) override;
    bool keyPress(const KeyEvent& event) override;

private:
    double position() const;
    double snapped(double value) const;
    RectF handleRect() const;
    void moveTo(double value);

    double m_from = 0;
    double m_to = 1;
    double m_step = 0;
    double m_value = 0;
    bool m_pressed = false;
    double m_grabOffset = 0;
};

} // namespace cutefish::ui
