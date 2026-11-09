#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/TextureAtlas.h>

namespace Elixir::Aether::Effect
{
    /**
     * @brief Stores material authoring data serialized in an Aether effect.
     *
     * This structure represents the material data embedded in an effect asset. It
     * is not a runtime Material, MaterialInstance, or GPU render proxy.
     *
     * Effect::MaterialResolver converts this description into a material when the
     * owning System is compiled.
     */
    struct SMaterialDescription
    {
        glm::vec3 BaseColor{ 1.0f };
        float Opacity = 1.0f;
        glm::vec3 Emissive{ 0.0f };
        std::string BaseColorTexturePath;

        /** Atlas layout used by base-color and emissive textures. */
        Materials::STextureAtlas Atlas;
        /** Optional emissive texture, sampled using the same atlas frame. */
        std::string EmissiveTexturePath;
        /** Multiplier applied to the emissive texture's RGB values. */
        float EmissiveScale = 1;
        /** Optional horizontal color ramp indexed by base-texture luminance. */
        std::string ColorRampTexturePath;
        /** Optional normal map used for authored directional color modulation. */
        std::string NormalTexturePath;
        /** Light direction expressed in the normal map's coordinate system. */
        glm::vec3 NormalLightDirection{-0.4f, 0.6f, 0.7f};
        /** Minimum normal-map lighting contribution, from zero to one. */
        float AmbientLight = 0.35f;
        /** Optional normal map providing screen-space refraction offsets. */
        std::string RefractionTexturePath;
        /** Refraction offset multiplier in normalized screen coordinates. */
        float RefractionStrength = 0.02f;

        /**
         * @brief Compares all authored properties for material reuse.
         * @param other Description to compare.
         */
        bool operator==(const SMaterialDescription& other) const = default;

        /** Controls blending and depth writes; existing effects default to translucent. */
        Materials::EMaterialBlendMode BlendMode = Materials::EMaterialBlendMode::Translucent;
    };
}
