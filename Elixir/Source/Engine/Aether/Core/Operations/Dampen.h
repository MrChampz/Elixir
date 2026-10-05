#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Drag argument for Dampen. */
    inline constexpr uint32_t Dampen_Drag = 0;
}

namespace Elixir::Aether::Core::Operations::Dampen
{
    /** @brief Returns the input definition and destination rules for Dampen. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Scalar };
        static const SOperationDefinition Definition{
            EParticleOp::Dampen, Inputs, false,
            false, true
        };
        return Definition;
    }
}
