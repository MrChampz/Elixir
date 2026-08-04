#pragma once

#include <Engine/Aether/Particle.h>
#include <Engine/Aether/ParticleMaterialDefinition.h>
#include <Engine/Material/Material.h>

namespace Elixir::Aether
{
    EMaterialUsage GetParticleMaterialUsage(const EParticleRenderMode mode);

    Ref<Material> CreateParticleMaterial(
        std::string name,
        EParticleRenderMode renderMode,
        const SParticleMaterialDefinition& definition
    );
}
