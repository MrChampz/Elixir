#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Graphics/TextureAtlas.h>

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
        std::string BaseColorTexturePath;

        glm::vec3 Emissive{ 0.0f };
        std::string EmissiveTexturePath;
        float EmissiveScale = 1;

        /** Optional tangent-space normal map connected to the material normal channel. */
        std::string NormalTexturePath;

        /** Optional horizontal color ramp indexed by base-texture luminance. */
        std::string ColorRampTexturePath;

        /** Optional normal map providing screen-space refraction offsets. */
        std::string RefractionTexturePath;

        /** Refraction offset multiplier in normalized screen coordinates. */
        float RefractionStrength = 0.02f;

        float Opacity = 1.0f;

        /** Atlas layout used by base-color and emissive textures. */
        STextureAtlasLayout Atlas;

        /** Interpolates adjacent atlas frames when sampling the material. */
        bool InterpolateAtlas = false;

        /** Selects whether the material responds to renderer lighting. */
        Materials::EMaterialShadingModel ShadingModel = Materials::EMaterialShadingModel::Lit;

        /** Controls blending and depth writes; existing effects default to translucent. */
        Materials::EMaterialBlendMode BlendMode = Materials::EMaterialBlendMode::Translucent;

        /**
         * @brief Compares all authored properties for material reuse.
         * @param other Description to compare.
         */
        bool operator==(const SMaterialDescription& other) const = default;
    };
}
