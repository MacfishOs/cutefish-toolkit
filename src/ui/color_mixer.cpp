// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/color_mixer.h"

#include <utility>

#include "ui/color_provider.h"

namespace cutefish::ui {

ColorRecipe::ColorRecipe(Color color)
    : m_make([color](const ColorProvider&) { return color; })
{
}

ColorRecipe::ColorRecipe(std::function<Color(const ColorProvider&)> make)
    : m_make(std::move(make))
{
}

std::function<Color(const ColorProvider&)> ColorRecipe::reference(uint16_t id)
{
    return [id](const ColorProvider& provider) { return provider.get(id); };
}

Color ColorRecipe::resolve(const ColorProvider& provider) const
{
    return m_make(provider);
}

ColorRecipe withAlpha(const ColorRecipe& recipe, double alpha)
{
    return ColorRecipe([recipe, alpha](const ColorProvider& provider) {
        return recipe.resolve(provider).withAlpha(alpha);
    });
}

ColorRecipe lighter(const ColorRecipe& recipe, double factor)
{
    return ColorRecipe([recipe, factor](const ColorProvider& provider) {
        return recipe.resolve(provider).lighter(factor);
    });
}

ColorRecipe darker(const ColorRecipe& recipe, double factor)
{
    return ColorRecipe([recipe, factor](const ColorProvider& provider) {
        return recipe.resolve(provider).darker(factor);
    });
}

ColorRecipe& ColorMixer::recipe(uint16_t id)
{
    return m_recipes.try_emplace(id, Color{}).first->second;
}

} // namespace cutefish::ui
