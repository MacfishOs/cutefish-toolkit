// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

#include "ui/label.h"

namespace cutefish::ui {

// A Label with Qt Quick Text's style: the text drawn first in the style
// colour one logical pixel down (Text.Raised) or up (Text.Sunken), then
// the text over it. Layout and size are a Label's.
class StyledLabel : public Label
{
public:
    enum class Style {
        Normal,
        Raised,
        Sunken,
    };

    explicit StyledLabel(std::string text = {});

    void setStyle(Style style, Color color);
    Style style() const { return m_style; }

    RectF visualRect() const override;

protected:
    void paint(Painter& painter) override;

private:
    Style m_style = Style::Normal;
    Color m_styleColor;
};

} // namespace cutefish::ui
