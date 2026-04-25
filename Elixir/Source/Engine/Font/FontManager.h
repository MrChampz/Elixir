#pragma once

#include "Engine/Graphics/TextureSet.h"

#include <Engine/Font/Font.h>
#include <Engine/Font/FontBackend.h>

namespace Elixir
{
    class ELIXIR_API FontManager
    {
      public:
        static constexpr std::string_view DEFAULT_FONT_NAME = "SF-Pro-Display-Regular";

        static void Initialize(const GraphicsContext* context);
        static void Shutdown();

        /**
         * Get the default font.
         * @return A reference to the default font.
         */
        static Ref<Font> GetDefaultFont();

        /**
         * Get a loaded font by name.
         * @param name The name of the font to retrieve.
         * @return A reference to the font with the specified name, or nullptr if the font is
         * not found.
         */
        static Ref<Font> GetFont(const std::string& name);

        /**
         * Load a font from a file path. Supported formats: (TTF, OTF).
         * @param filepath The file path to the font file to be loaded.
         * @return A reference to the loaded font, or nullptr if the font cannot be loaded.
         */
        static Ref<Font> Load(const std::filesystem::path& filepath);

        /**
         * Measure the width and height of the rendered text in pixels, based on the font
         * metrics.
         * @param text The text to be measured
         * @param font The font used to display the text
         * @param fontSize The font size in pixels
         * @return A 2d vector containing the width and height of the rendered text in pixels.
         */
        static glm::vec2 MeasureText(
            const std::string& text,
            const Ref<Font>& font,
            float fontSize
        );

        /**
         * Get the line height in pixels, which is the distance from the baseline of one
         * line of text.
         * @param font The font used to display the text
         * @param fontSize The font size in pixels
         * @return The line height in pixels, which is the distance from the baseline of one
         * line of text to the baseline of the next line of text.
         */
        static float GetLineHeight(const Ref<Font>& font, float fontSize);

        /**
         * Get the TextureSet containing all loaded font atlases.
         * @return The TextureSet containing all loaded font atlases.
         */
        static Ref<TextureSet> GetAtlasesTextureSet() { return s_FontsAtlases; }

      private:
        FontManager() = delete;
        FontManager(const FontManager&) = delete;
        FontManager& operator=(const FontManager&) = delete;

        static bool s_Initialized;
        static Scope<FontBackend> s_FontBackend;
        static Ref<TextureSet> s_FontsAtlases;
        static std::unordered_map<std::string_view, Ref<Font>> s_Fonts;
        static const GraphicsContext* s_GraphicsContext;
    };
}
