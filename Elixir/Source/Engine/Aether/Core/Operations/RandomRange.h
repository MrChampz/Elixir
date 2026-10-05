#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Minimum argument for RandomRange. */
    inline constexpr uint32_t RandomRange_Minimum = 0;
    /** @brief Position of the Maximum argument for RandomRange. */
    inline constexpr uint32_t RandomRange_Maximum = 1;
}

namespace Elixir::Aether::Core::Operations::RandomRange
{
    /** @brief Returns the input definition and destination rules for RandomRange. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Target, EOperandType::Target };
        static const SOperationDefinition Definition{
            EParticleOp::RandomRange, Inputs, false,
            true, false
        };
        return Definition;
    }
}
