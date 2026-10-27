#pragma once

#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/Sampler.h>

namespace Elixir
{
    /**
     * @brief Stores image-based lighting resources for Surface materials.
     *
     * All textures use equirectangular projection. The prefiltered texture stores
     * roughness-filtered specular reflections.
     */
    struct SSurfaceEnvironment
    {
        /** Raw environment texture used for sharp reflections. */
        Ref<Texture> Environment;

        /** Diffuse irradiance texture derived from @ref Environment. */
        Ref<Texture> Irradiance;

        /** Roughness-filtered specular reflection texture. */
        Ref<Texture> Prefiltered;

        /** Sampler used by all environment textures. */
        Ref<Sampler> Sampler;

        /** Multiplier applied to environment lighting. */
        float Intensity = 1.0f;

        /** Highest valid mip level in @ref Environment. */
        float MaxLod = 0.0f;
    };

    /** @brief Selects the Surface lighting contribution shown by the renderer. */
    enum class ESurfaceDebugView : uint32_t
    {
        /** Render the fully lit material. */
        Composite,

        /** Render the material base color before lighting. */
        BaseColor,

        /** Render the diffuse contribution from image-based lighting. */
        DiffuseIBL,

        /** Render the specular contribution from image-based lighting. */
        SpecularIBL,

        /** Render the diffuse contribution from the directional light. */
        DirectDiffuse,

        /** Render the specular contribution from the directional light. */
        DirectSpecular,

        /** Render the clear-coat reflection and direct-light contribution. */
        ClearCoat,
    };

    /** @brief Stores frame lighting used by mesh Surface materials. */
    struct SMeshLighting
    {
        /** Image-based lighting resources for the frame. */
        SSurfaceEnvironment Environment;

        /** Direction from the shared point toward the directional light. */
        glm::vec3 DirectionalLightDirection{ -0.5f, 0.65f, -0.55f };

        /** Linear RGB color of the directional light. */
        glm::vec3 DirectionalLightColor{ 1.0f, 0.96f, 0.9f };

        /** Intensity multiplier of the directional light. */
        float DirectionalLightIntensity = 2.2f;

        /** Surface lighting contribution to display for diagnostics. */
        ESurfaceDebugView DebugView = ESurfaceDebugView::Composite;
    };

}
