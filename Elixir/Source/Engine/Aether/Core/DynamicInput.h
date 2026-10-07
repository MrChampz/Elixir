#pragma once

#include <cstdint>

namespace Elixir::Aether::Core
{
    /** @brief Selects a runtime value used by a particle module or curve. */
    enum class EDynamicInput : uint8_t
    {
        None = 0,
        DeltaTime,
        NormalizedAge,
        EmitterTime,
        Random,
        ParticleSeed
    };
}
