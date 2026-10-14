// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <string>

#include "ui/toggle_control.h"

namespace cutefish::ui {

class Animation;

// FishUI's switch: a pill in the accent colour when on, its white handle
// sliding across.
class Switch : public ToggleControl
{
public:
    explicit Switch(std::string text = {});
    ~Switch() override;

protected:
    void checkedChanged(bool animated) override;
    void paintIndicator(Painter& painter, const RectF& rect) override;

private:
    // 0 off, 1 on; animated between.
    double m_handle = 0;
    double m_color = 0;
    double m_handleFrom = 0;
    double m_colorFrom = 0;
    std::unique_ptr<Animation> m_handleAnimation;
    std::unique_ptr<Animation> m_colorAnimation;
};

} // namespace cutefish::ui
