#pragma once

#include <Engine/Aether/Core/Particle.h>
#include <Engine/Aether/Effect/MaterialDescription.h>
#include <Engine/Material/Material.h>

namespace Elixir::Aether::Effect
{
    EMaterialUsage GetMaterialUsage(Core::EParticleRenderMode mode);

    Ref<Material> CreateMaterial(
        std::string name,
        Core::EParticleRenderMode renderMode,
        const SMaterialDescription& desc
    );
}
