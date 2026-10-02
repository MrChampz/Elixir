#pragma once

#include <Engine/Aether/Core/Particle.h>
#include <Engine/Aether/Effect/MaterialDescription.h>
#include <Engine/Materials/Material.h>

namespace Elixir::Aether::Effect
{
    using namespace Materials;

    /**
     * @brief Creates a material from Aether effect authoring data.
     *
     * The function creates a raw Material with Particle usage and builds its material
     * graph from desc. BaseColor, Opacity, and Emissive become
     * constant graph inputs.
     *
     * A non-empty BaseColorTexturePath creates a texture
     * parameter and multiplies its sampled RGB and alpha values into BaseColor and
     * Opacity, respectively.
     *
     * @param name Name assigned to the created material.
     * @param desc Serialized material data from the effect asset.
     * @return A new unregistered Material.
     *
     * @note The caller owns registration and later creation of a MaterialInstance.
     */
    Ref<Material> CreateMaterial(
        std::string name,
        const SMaterialDescription& desc
    );
}
