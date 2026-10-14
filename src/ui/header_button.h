// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <string>

#include "ui/view.h"

namespace cutefish::ui {

// FishUI's title bar button: window button artwork on a round plate that
// shows on hover, dimmed while the window is inactive.
class HeaderButton : public View
{
public:
    // An artwork such as "close", "minimize", "maximize", "restore", "add" or "menu".
    explicit HeaderButton(std::string artwork = {}, double size = 32);

    void setArtwork(std::string artwork);
    const std::string& artwork() const { return m_artwork; }
    // One of FishUI's glyphs instead of window artwork, such as "chevron-left".
    void setGlyph(std::string name, double size = 16);

    SizeF sizeHint() const override { return {m_size, m_size}; }
    bool acceptsPointer() const override { return true; }

    std::function<void()> onClicked;

protected:
    // The artwork drawn now; a button may pick it from the window's state.
    virtual std::string currentArtwork() const { return m_artwork; }
    void paint(Painter& painter) override;
    void pointerEnter() override;
    void pointerLeave() override;
    void pointerPress(const PointerEvent& event) override;
    void pointerRelease(const PointerEvent& event) override;

private:
    std::string m_artwork;
    std::string m_glyph;
    double m_glyphSize = 16;
    double m_size;
    bool m_hovered = false;
    bool m_pressed = false;
};

} // namespace cutefish::ui
