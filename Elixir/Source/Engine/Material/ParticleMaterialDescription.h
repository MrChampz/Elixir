#pragma once

#include <Engine/Graphics/Texture.h>
#include <Engine/Material/Material.h>

namespace Elixir
{
    // Immutable authoring values used to create one particle material.
    // They are resolved before this layer is called and become graph constants.
    struct SParticleMaterialDescription
    {
        EMaterialUsage Usage = EMaterialUsage::ParticleSprite;
        glm::vec3 BaseColor{ 1.0f };
        float Opacity = 1.0f;
        glm::vec3 Emissive{ 0.0f };
        Ref<Texture> Texture;
    };
}