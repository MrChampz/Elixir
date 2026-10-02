#pragma once

#include <Engine/Graphics/Sampler.h>
#include <Engine/Graphics/Texture.h>

#include <filesystem>

namespace Elixir
{
    class GraphicsContext;
}

/**
 * @brief Stores image-based lighting textures used by Dissolve.
 *
 * The textures use equirectangular projection. The irradiance and prefiltered
 * textures are derived from the source HDR environment when it is loaded.
 */
class Environment final
{
  public:
    /** @brief Number of roughness levels stored in the prefiltered texture. */
    static constexpr uint32_t PrefilterLevels = 6;

    /**
     * @brief Load an HDR environment and create its lighting textures.
     * @param context Graphics context used to create the textures and sampler.
     * @param path Path to an HDR equirectangular image.
     * @return The loaded environment, or null when the image cannot be loaded.
     */
    static Scope<Environment> Load(
        const GraphicsContext& context,
        const std::filesystem::path& path
    );

    /** @brief Return the source HDR environment texture. */
    const Ref<Texture>& GetEnvironment() const { return m_Environment; }

    /** @brief Return the diffuse irradiance texture derived from the environment. */
    const Ref<Texture>& GetIrradiance() const { return m_Irradiance; }

    /** @brief Return the roughness-filtered reflection texture. */
    const Ref<Texture>& GetPrefiltered() const { return m_Prefiltered; }

    /** @brief Return the sampler shared by the environment textures. */
    const Ref<Sampler>& GetSampler() const { return m_Sampler; }

    /** @brief Return the highest roughness level in the prefiltered texture. */
    static float GetMaxLod() { return static_cast<float>(PrefilterLevels - 1); }

  private:
    Environment() = default;

    Ref<Texture> m_Environment;
    Ref<Texture> m_Irradiance;
    Ref<Texture> m_Prefiltered;
    Ref<Sampler> m_Sampler;
};
