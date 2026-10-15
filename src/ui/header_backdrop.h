// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include "base/signal.h"
#include "ui/view.h"

namespace cutefish::ui {

class ScrollView;

// FishUI's HeaderBackdrop: the frosted bar over the top or bottom of a
// scrolling view, the content scrolled beneath it blurred as MultiEffect
// (blurMax 32) blurs it, under a translucent tint and a hairline. At rest it
// is invisible; it fades in over 16 pixels of content scrolling under it.
// It lies over the view, which it must not be inside.
class HeaderBackdrop : public View
{
public:
    enum class Edge {
        Top,
        Bottom,
    };

    explicit HeaderBackdrop(ScrollView* view, Edge edge = Edge::Top);
    ~HeaderBackdrop() override;

    void setSeparatorVisible(bool visible);
    // What the view's content is drawn on; the theme's background unless set.
    void setBaseColor(std::optional<Color> color);
    // The tint, at 0.72; the theme's second background unless set.
    void setTintColor(std::optional<Color> color);

protected:
    void paint(Painter& painter) override;

private:
    double fade() const;

    ScrollView* m_view;
    Edge m_edge;
    bool m_separatorVisible = true;
    std::optional<Color> m_baseColor;
    std::optional<Color> m_tintColor;
    base::Subscription m_scrolled;
};

} // namespace cutefish::ui
