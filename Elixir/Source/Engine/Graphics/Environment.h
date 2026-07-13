#pragma once

#include <Engine/Graphics/Texture.h>

namespace Elixir
{
    // A loaded HDR equirectangular environment for image-based lighting: the raw
    // env map (for specular reflections) plus a CPU-baked diffuse irradiance map.
    class ELIXIR_API Environment
    {
      public:
        static Ref<Environment> Load(const GraphicsContext* context, const std::filesystem::path& hdrPath);

        [[nodiscard]] const Ref<Texture>& GetEnvironment() const { return m_Environment; }
        [[nodiscard]] const Ref<Texture>& GetIrradiance() const { return m_Irradiance; }

      private:
        Ref<Texture> m_Environment;
        Ref<Texture> m_Irradiance;
    };
}
