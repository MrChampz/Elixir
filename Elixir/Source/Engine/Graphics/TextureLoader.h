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

    private:
        static const GraphicsContext* s_Context;
        static bool s_Initialized;
    };
}
