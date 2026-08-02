#pragma once

#include <Engine/Aether/System.h>

namespace Elixir
{
    class ParticleMaterialLibrary;
}

namespace Elixir::Aether
{
    ELIXIR_API Ref<System> LoadEffectFile(
        const std::filesystem::path& filepath,
        const ParticleMaterialLibrary& materials
    );
}