#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Minimum argument for Clamp. */
    inline constexpr uint32_t Clamp_Minimum = 0;
    /** @brief Position of the Maximum argument for Clamp. */
    inline constexpr uint32_t Clamp_Maximum = 1;
}

namespace Elixir::Aether::Core::Operations::Clamp
{
    /** @brief Returns the input definition and destination rules for Clamp. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Target, EOperandType::Target };
        static const SOperationDefinition Definition{
            EParticleOp::Clamp, Inputs, false,
            false, true
        };
        return Definition;
    }
}
