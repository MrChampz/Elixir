#pragma once

#include <Engine/Font/Font.h>

namespace Elixir
{
    class ELIXIR_API FontBackend
    {
    public:
        virtual ~FontBackend() = default;

        /**
         * Load a font from a file path. Supported formats: (TTF, OTF).
         * @param filepath The file path to the font file to be loaded.
         * @return A reference to the loaded font, or nullptr if the font cannot be loaded.
         */
        virtual Ref<SFont> Load(const std::filesystem::path& filepath) = 0;

        /**
         * Measure the width and height of the rendered text in pixels, based on the font
         * metrics.
         * @param text The text to be measured
         * @param font The font used to display the text
         * @param fontSize The font size in pixels
         * @return A 2d vector containing the width and height of the rendered text in pixels.
         */
        virtual glm::vec2 MeasureText(
            const std::string& text,
            const Ref<SFont>& font,
            float fontSize
        ) = 0;

        /**
         * Get the line height in pixels, which is the distance from the baseline of one
         * line of text.
         * @param font The font used to display the text
         * @param fontSize The font size in pixels
         * @return The line height in pixels, which is the distance from the baseline of one
         * line of text to the baseline of the next line of text.
         */
        virtual float GetLineHeight(const Ref<SFont>& font, float fontSize) = 0;
    };
}