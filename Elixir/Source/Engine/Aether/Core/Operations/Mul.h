#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Value argument for Mul. */
    inline constexpr uint32_t Mul_Value = 0;
}

namespace Elixir::Aether::Core::Operations::Mul
{
    /** @brief Returns the input definition and destination rules for Mul. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Target };
        static const SOperationDefinition Definition{
            EParticleOp::Mul, Inputs, false,
            false, true
        };
        return Definition;
    }
}
