#pragma once

#include <Engine/Core/Core.h>
#include <Engine/GUI/Style.h>

namespace Elixir::GUI::Util
{
    /** @brief Interpolate two scalar style values. */
    inline float Interpolate(float from, float to, float amount)
    {
        return glm::mix(from, to, amount);
    }

    /** @brief Interpolate two two-dimensional style values. */
    inline glm::vec2 Interpolate(const glm::vec2& from, const glm::vec2& to, float amount)
    {
        return glm::mix(from, to, amount);
    }

    /** @brief Interpolate two four-dimensional style values. */
    inline glm::vec4 Interpolate(const glm::vec4& from, const glm::vec4& to, float amount)
    {
        return glm::mix(from, to, amount);
    }

    /** @brief Interpolate two colors. */
    inline SColor Interpolate(const SColor& from, const SColor& to, float amount)
    {
        return {
            Interpolate(from.R, to.R, amount),
            Interpolate(from.G, to.G, amount),
            Interpolate(from.B, to.B, amount),
            Interpolate(from.A, to.A, amount),
        };
    }

    /** @brief Interpolate an outline's color and thickness. */
    inline SOutline Interpolate(const SOutline& from, const SOutline& to, float amount)
    {
        return { Interpolate(from.Color, to.Color, amount), Interpolate(from.Thickness, to.Thickness, amount) };
    }

    /** @brief Interpolate the scalar and color properties of a brush. */
    inline SBrush Interpolate(const SBrush& from, const SBrush& to, float amount)
    {
        return {
            .Color = Interpolate(from.Color, to.Color, amount),
            .Texture = amount < 0.5f ? from.Texture : to.Texture,
            .Borders = Interpolate(from.Borders, to.Borders, amount),
            .CornerRadius = Interpolate(from.CornerRadius, to.CornerRadius, amount),
            .Outline = Interpolate(from.Outline, to.Outline, amount),
            .InsetShadow = Interpolate(from.InsetShadow, to.InsetShadow, amount),
            .DropShadow = Interpolate(from.DropShadow, to.DropShadow, amount),
        };
    }
}
