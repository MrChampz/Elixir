#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Value argument for SetLiteral. */
    inline constexpr uint32_t SetLiteral_Value = 0;
}

namespace Elixir::Aether::Core::Operations::SetLiteral
{
    /** @brief Returns the input definition and destination rules for SetLiteral. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Target };
        static const SOperationDefinition Definition{
            EParticleOp::SetLiteral, Inputs, true,
            true, true
        };
        return Definition;
    }
}
