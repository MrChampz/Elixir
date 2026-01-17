#pragma once

#include <Engine/Graphics/Texture.h>
#include <Engine/GUI/Types.h>

namespace Elixir::GUI
{
    struct SGlyph
    {
        int Unicode;
        float Advance;
        std::optional<SRect> PlaneBounds;
        std::optional<SRect> AtlasBounds;
    };

    struct SAtlasInfo
    {
        float AscenderY;
        float DescenderY;
        int Width;
        int Height;
    };

    struct SAtlas
    {
        SAtlasInfo Info;
        Ref<Texture> Texture;
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

    class ELIXIR_API FontManager
    {
      public:
        static void Initialize(const GraphicsContext* context);
        static void Shutdown();

        static Ref<SFont> Load(const std::filesystem::path& filepath);

      private:
        FontManager() = delete;
        FontManager(const FontManager&) = delete;
        FontManager& operator=(const FontManager&) = delete;

        static bool s_Initialized;
        static std::unordered_map<std::string, Ref<SFont>> s_Fonts;
        static const GraphicsContext* s_GraphicsContext;
    };
}
