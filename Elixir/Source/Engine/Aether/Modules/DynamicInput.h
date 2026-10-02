#pragma once

#include <cstdint>

namespace Elixir::Aether::Modules
{
    /** @brief Selects a runtime value used by a particle module or curve. */
    enum class EDynamicInput : uint32_t
    {
        None = 0,
        DeltaTime,
        NormalizedAge,
        EmitterTime,
        Random,
        ParticleSeed
    };
}
