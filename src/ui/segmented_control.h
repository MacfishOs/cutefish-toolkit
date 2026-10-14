// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ui/view.h"

namespace cutefish::ui {

class Label;

// A row of equal buttons, one of them current: FishUI's segmented control, a
// pill, or its tab bar, a rounded box.
class SegmentedControl : public View
{
public:
    enum class Style {
        Segmented,
        Tabs,
    };

    explicit SegmentedControl(std::vector<std::string> items = {}, Style style = Style::Segmented);

    void setItems(std::vector<std::string> items);
    size_t count() const { return m_labels.size(); }
    int currentIndex() const { return m_current; }
    // Does not call onCurrentChanged.
    void setCurrentIndex(int index);

    SizeF sizeHint() const override;
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

    // After the user picked a button.
    std::function<void(int index)> onCurrentChanged;

protected:
    void layout() override;
    void paint(Painter& painter) override;
    void pointerPress(const PointerEvent& event) override;
    void pointerRelease(const PointerEvent& event) override;
    bool keyPress(const KeyEvent& event) override;

private:
    RectF itemRect(size_t index) const;
    int itemAt(PointF point) const;
    void pick(int index);
    void updateColors();

    Style m_style;
    std::vector<Label*> m_labels;
    int m_current = 0;
    int m_pressed = -1;
};

// FishUI's tab bar.
class TabBar : public SegmentedControl
{
public:
    explicit TabBar(std::vector<std::string> items = {})
        : SegmentedControl(std::move(items), Style::Tabs)
    {
    }
};

} // namespace cutefish::ui
