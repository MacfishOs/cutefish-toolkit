// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>

#include "ui/view.h"

namespace cutefish::ui {

// One of FishUI's single-colour glyphs, such as "check", "chevron-down" or
// "search", in a colour; the theme's text colour unless set. Drawn square,
// centred and whole on device pixels.
class Icon : public View
{
public:
    explicit Icon(std::string name = {}, double size = 16);

    // Adds an application's own single-colour glyph, drawn tinted like the
    // built-in ones; the SVG's contents, not a file name.
    static void registerGlyph(const std::string& name, std::string svg);

    void setIconName(std::string name);
    const std::string& iconName() const { return m_name; }
    void setColor(std::optional<Color> color);
    void setSize(double size);

    SizeF sizeHint() const override { return {m_size, m_size}; }

protected:
    void paint(Painter& painter) override;

private:
    std::string m_name;
    std::optional<Color> m_color;
    double m_size;
};

// An icon from the user's icon theme, by its freedesktop name, such as an
// application's icon.
class ThemeIcon : public View
{
public:
    explicit ThemeIcon(std::string name = {}, double size = 32);

    void setIconName(std::string name);
    const std::string& iconName() const { return m_name; }
    void setSize(double size);
    // The file the theme gives for the name at this size, at the scale.
    std::string path(double scale = 1) const;
    // As an Image of image://icontheme with this sourceSize, rather than an
    // IconItem: FishUI's provider asks QIcon for the device-pixel size at the
    // application's integer scale, and the larger pixmap is filtered down.
    void setImageProviderLookup(bool provider);

    SizeF sizeHint() const override { return {m_size, m_size}; }

protected:
    void paint(Painter& painter) override;

private:
    std::string m_name;
    double m_size;
    bool m_provider = false;
};

} // namespace cutefish::ui
