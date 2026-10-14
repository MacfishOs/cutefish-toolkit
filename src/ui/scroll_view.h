// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <utility>

#include "ui/view.h"

namespace cutefish::ui {

class ScrollBar;

// A viewport onto a larger content view, moved by the touchpad, the wheel and
// its scroll bars, with cutefish-framework's touchpad physics. It scrolls
// vertically by default: the content then gets the viewport's width and the
// height it asks for at that width.
class ScrollView : public View
{
public:
    ScrollView();
    ~ScrollView() override;

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

    void setScrollDirections(bool horizontal, bool vertical);
    // Past the ends the content stretches and springs back.
    void setElastic(bool elastic);
    // Settles on whole viewport-sized pages.
    void setPaged(bool paged);

    // The content's offset under the viewport's top left.
    PointF scrollPosition() const { return m_position; }
    // Clamped to the content; stops a coast.
    void setScrollPosition(PointF position);
    // Scrolls as little as needed to show the rect, in content coordinates.
    void ensureVisible(const RectF& rect);
    SizeF contentSize() const { return m_contentSize; }

    std::function<void()> onScrolled;

protected:
    void layout() override;
    bool scroll(const ScrollEvent& event) override;
    // After every change of the position, before the frame is painted.
    virtual void positionChanged() {}

private:
    struct Private;

    void applyPosition(PointF position);
    void updateBars();
    PointF maximumPosition() const;

    std::unique_ptr<Private> d;
    View* m_content = nullptr;
    ScrollBar* m_verticalBar;
    ScrollBar* m_horizontalBar;
    PointF m_position;
    SizeF m_contentSize;
    bool m_horizontal = false;
    bool m_vertical = true;
};

} // namespace cutefish::ui
