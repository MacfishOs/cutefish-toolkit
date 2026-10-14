// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <utility>

#include "ui/view.h"

namespace cutefish::ui {

class HeaderButton;

// FishUI's window header: a bar that moves the window, toggles maximize on a
// double click, and holds the window buttons at its end, with room for a
// title or tools before them.
class WindowHeader : public View
{
public:
    explicit WindowHeader(double height = 40);

    // Fills the bar before the buttons.
    void setContentView(std::unique_ptr<View> view);
    template<typename T, typename... Args>
    T* setContent(Args&&... args)
    {
        auto view = std::make_unique<T>(std::forward<Args>(args)...);
        T* content = view.get();
        setContentView(std::move(view));
        return content;
    }
    View* contentView() const { return m_content; }

    void setMinimizeVisible(bool visible);
    void setMaximizeVisible(bool visible);
    void setCloseVisible(bool visible);
    // From the top and the end, FishUI's 4 by default and 8 in dialogs.
    void setButtonMargin(double margin);

    SizeF sizeHint() const override { return {0, m_height}; }
    bool acceptsPointer() const override { return true; }

protected:
    void layout() override;
    void pointerPress(const PointerEvent& event) override;

private:
    double m_height;
    double m_buttonMargin = 4;
    View* m_content = nullptr;
    HeaderButton* m_minimize;
    HeaderButton* m_maximize;
    HeaderButton* m_close;
};

} // namespace cutefish::ui
