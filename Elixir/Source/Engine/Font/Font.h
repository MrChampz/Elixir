#pragma once

#include <Engine/GUI/Definitions.h>
#include <Engine/Graphics/Definitions.h>
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
        int Width;
        int Height;
    };

    struct SAtlas
    {
        SAtlasInfo Info;
        Ref<Texture> MTSDF;
    };

    struct SFontCreateInfo
    {
        std::string Name;
        SAtlas Atlas;
        std::vector<SGlyph> Glyphs;
        float AscenderY;
        float DescenderY;
    };

    class Font
    {
        friend class FontManager;
      public:
        explicit Font(const SFontCreateInfo& info);

        /**
         * Measure the width and height of the rendered text in pixels, based on the font
         * metrics.
         * @param text The text to be measured
         * @param fontSize The font size in pixels
         * @return A 2d vector containing the width and height of the rendered text in pixels.
         */
        glm::vec2 MeasureText(const std::string& text, float fontSize) const;

        float GetScale() const;

        /**
         * Get the line height in pixels, which is the distance from the baseline of one
         * line of text.
         * @param fontSize The font size in pixels
         * @return The line height in pixels, which is the distance from the baseline of one
         * line of text to the baseline of the next line of text.
         */
        float GetLineHeight(float fontSize) const;

        /**
         * Get the glyph information for a given character.
         * @param codepoint The Unicode code point of the character.
         * @return An optional containing the glyph information for the given character,
         * or std::nullopt if the character is not found in the font.
         */
        std::optional<const SGlyph> GetGlyph(int codepoint) const;

        /**
         * Get the kerning adjustment in pixels between two characters, which is the amount of
         * horizontal space to add or subtract between the two characters when they are
         * rendered next to each other. A positive kerning value increases the space between
         * the characters, while a negative kerning value decreases the space between the
         * characters.
         * @param a The Unicode code point of the first character.
         * @param b The Unicode code point of the second character.
         * @return The kerning adjustment in pixels.
         */
        float GetKerning(int a, int b);

        /**
         * Set the kerning adjustment in pixels between two characters, which is the amount of
         * horizontal space to add or subtract between the two characters when they are
         * rendered next to each other. A positive kerning value increases the space between
         * the characters, while a negative kerning value decreases the space between the
         * characters.
         * @param a The Unicode code point of the first character.
         * @param b The Unicode code point of the second character.
         * @param kerning The kerning adjustment in pixels.
         */
        void SetKerning(int a, int b, float kerning);

        const std::string& GetName() const { return m_Name; }
        const SResourceHandle& GetAtlasHandle() const { return m_AtlasHandle; }
        const SAtlas& GetAtlas() const { return m_Atlas; }
        float GetAscenderY() const { return m_AscenderY; }
        float GetDescenderY() const { return m_DescenderY; }

        glm::vec2 GetUnitRange() const
        {
            return {
                m_Atlas.Info.PxRange / m_Atlas.Info.Width,
                m_Atlas.Info.PxRange / m_Atlas.Info.Height
            };
        }

        /**
         * Get the texture containing the multi-channel signed distance field (MTSDF) atlas
         * for this font.
         * @return The texture containing the MTSDF atlas for this font.
         */
        Ref<Texture2D> GetMTSDF() const { return std::dynamic_pointer_cast<Texture2D>(m_Atlas.MTSDF); }

      protected:
        void SetAtlasHandle(const SResourceHandle handle) { m_AtlasHandle = handle; }

      private:
        void InitGlyphs(const SFontCreateInfo& info);

        std::string m_Name;

        // A handle to the atlas in the font manager's texture set.
        SResourceHandle m_AtlasHandle;
        SAtlas m_Atlas = {};
        std::unordered_map<int, SGlyph> m_Glyphs;
        std::unordered_map<uint64_t, float> m_Kerning;

        float m_AscenderY = 0.0f;
        float m_DescenderY = 0.0f;
    };
}