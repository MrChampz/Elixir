#pragma once

#include <Engine/Graphics/Texture.h>

namespace Elixir
{
    class ELIXIR_API TextureLoader
    {
      public:
        static void Initialize(const GraphicsContext* context);

        static Ref<Texture> Load(
            const std::filesystem::path& path,
            EImageFormat format = EImageFormat::R8G8B8A8_SRGB
        );

        /**
         * @brief Loads a two-dimensional texture using the supplied configuration.
         * @param path Path to the source image.
         * @param info Texture configuration. Decoded pixels and dimensions replace
         * the corresponding fields before creation.
         */
        static Ref<Texture> Load(
            const std::filesystem::path& path,
            const STexture2DCreateInfo& info
        );

    private:
        static const GraphicsContext* s_Context;
        static bool s_Initialized;
    };
}
