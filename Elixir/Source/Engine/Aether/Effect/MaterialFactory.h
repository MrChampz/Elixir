#pragma once

#include <Engine/Aether/Core/Particle.h>
#include <Engine/Aether/Effect/MaterialDescription.h>
#include <Engine/Material/Material.h>

namespace Elixir::Aether::Effect
{
    /**
     * @brief Returns the material usage required by an Aether render mode.
     *
     * @param mode Particle geometry mode selected by an emitter.
     * @return Material usage compatible with the selected render mode.
     *
     * @note An invalid enum value falls back to EMaterialUsage::ParticleSprite.
     */
    EMaterialUsage GetMaterialUsage(Core::EParticleRenderMode mode);

    /**
     * @brief Creates a material from Aether effect authoring data.
     *
     * The function creates a raw Material with the usage required by renderMode and
     * builds its material graph from desc. BaseColor, Opacity, and Emissive become
     * constant graph inputs.
     *
     * For sprite emitters, a non-empty BaseColorTexturePath creates a texture
     * parameter and multiplies its sampled RGB and alpha values into BaseColor and
     * Opacity, respectively.
     *
     * @param name Name assigned to the created material.
     * @param renderMode Particle geometry mode that determines material usage.
     * @param desc Serialized material data from the effect asset.
     * @return A new unregistered Material.
     *
     * @note The caller owns registration and later creation of a MaterialInstance.
     */
    Ref<Material> CreateMaterial(
        std::string name,
        Core::EParticleRenderMode renderMode,
        const SMaterialDescription& desc
    );
}
