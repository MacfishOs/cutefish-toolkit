// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "ui/color_id.h"
#include "ui/geometry.h"

namespace cutefish::ui {

class ColorProvider;
struct Theme;

// How a colour is made: a colour, another colour by its id, or a transform
// of one of these. Ids resolve against the provider's finished table, so a
// colour made from another follows a later mixer that replaces that one.
class ColorRecipe
{
public:
    ColorRecipe(Color color);
    template<ColorIdType Id>
    ColorRecipe(Id id)
        : m_make(reference(uint16_t(id)))
    {
    }
    explicit ColorRecipe(std::function<Color(const ColorProvider&)> make);

    Color resolve(const ColorProvider& provider) const;

private:
    static std::function<Color(const ColorProvider&)> reference(uint16_t id);

    std::function<Color(const ColorProvider&)> m_make;
};

// The colour with its alpha replaced, as Color::withAlpha.
ColorRecipe withAlpha(const ColorRecipe& recipe, double alpha);
ColorRecipe lighter(const ColorRecipe& recipe, double factor);
ColorRecipe darker(const ColorRecipe& recipe, double factor);

// The recipes one mixer gives; a recipe replaces one an earlier mixer gave
// for the same colour.
class ColorMixer
{
public:
    template<ColorIdType Id>
    ColorRecipe& operator[](Id id)
    {
        return recipe(uint16_t(id));
    }

private:
    friend class ColorProvider;

    ColorRecipe& recipe(uint16_t id);

    std::unordered_map<uint16_t, ColorRecipe> m_recipes;
};

// Gives a mixer its recipes for the theme.
using ColorMixerFunction = std::function<void(ColorMixer& mixer, const Theme& theme)>;

} // namespace cutefish::ui
