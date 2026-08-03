#pragma once

#include <Engine/Material/ParticleMaterialDescription.h>

namespace Elixir
{
    // Authoring detail of the engine Sprite graph. It is not a renderer ABI.
    inline constexpr std::string_view DEFAULT_SPRITE_TEXTURE_PARAMETER = "SpriteTexture";

    // Creates the engine-owned source material used when a particle emitter
    // has no authored material proxy for a supported particle usage.
    Ref<Material> CreateParticleMaterial(const SParticleMaterialDescription& desc);
}