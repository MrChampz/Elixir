#pragma once

#include <Engine/GUI/Definitions.h>
#include <Engine/Graphics/Texture.h>

namespace Elixir
{
    using namespace Elixir::GUI;

    struct SGlyph
    {
        int Unicode;
        float Advance;
        std::optional<SRect> PlaneBounds;
        std::optional<SRect> AtlasBounds;
    };

    struct SAtlasInfo
    {
        float PxRange;
        float AscenderY;
        float DescenderY;
        int Width;
        int Height;
    };

    struct SAtlas
    {
        SAtlasInfo Info;
        Ref<Texture> MTSDF;
    };

    struct SFont
    {
        std::string Name;

        SAtlas Atlas;
        std::vector<SGlyph> Glyphs;

        std::optional<SGlyph> GetGlyph(const char character) const
        {
            const auto found = std::ranges::find_if(Glyphs, [character](const SGlyph& glyph)
            {
                return glyph.Unicode == static_cast<int>(character);
            });

            return found != Glyphs.end() ? std::optional{ *found } : std::nullopt;
        }
    };
}