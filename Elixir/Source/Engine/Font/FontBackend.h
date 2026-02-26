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
        virtual Ref<Font> Load(const std::filesystem::path& filepath) = 0;
    };
}