#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Value argument for AddWithDelta. */
    inline constexpr uint32_t AddWithDelta_Value = 0;
    /** @brief Position of the Multiplier argument for AddWithDelta. */
    inline constexpr uint32_t AddWithDelta_Multiplier = 1;
}

namespace Elixir::Aether::Core::Operations::AddWithDelta
{
    /** @brief Returns the input definition and destination rules for AddWithDelta. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Target, EOperandType::Scalar };
        static const SOperationDefinition Definition{
            EParticleOp::AddWithDelta, Inputs, false,
            false, true
        };
        return Definition;
    }
}
