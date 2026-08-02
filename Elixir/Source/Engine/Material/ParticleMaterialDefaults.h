#pragma once

#include <Engine/Material/Material.h>

namespace Elixir
{
    // Creates the engine-owned source material used when a particle emitter
    // has no authored material proxy for a supported particle usage.
    Ref<Material> CreateDefaultParticleMaterial(EMaterialUsage usage);
}