#pragma once

#include <Engine/Graphics/Texture.h>

namespace Elixir
{
    // A loaded HDR equirectangular environment for image-based lighting: the raw
    // env map (sharp specular / clear coat), a CPU-baked diffuse irradiance map,
    // and a CPU GGX-prefiltered specular map for rough reflections.
    class ELIXIR_API Environment
    {
      public:
        // The prefiltered specular map is an equirect atlas: PrefilterLevels blocks
        // stacked vertically, block k convolved for roughness k/(PrefilterLevels-1).
        // The shader mirrors these constants.
        static constexpr int PrefilterLevels = 6;
        static constexpr int PrefilterWidth = 128;
        static constexpr int PrefilterHeight = 64;

        static Ref<Environment> Load(const GraphicsContext* context, const std::filesystem::path& hdrPath);

        [[nodiscard]] const Ref<Texture>& GetEnvironment() const { return m_Environment; }
        [[nodiscard]] const Ref<Texture>& GetIrradiance() const { return m_Irradiance; }
        [[nodiscard]] const Ref<Texture>& GetPrefiltered() const { return m_Prefiltered; }

      private:
        Ref<Texture> m_Environment;
        Ref<Texture> m_Irradiance;
        Ref<Texture> m_Prefiltered;
    };
}
