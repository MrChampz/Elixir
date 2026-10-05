#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Value argument for Add. */
    inline constexpr uint32_t Add_Value = 0;
}

namespace Elixir::Aether::Core::Operations::Add
{
    /** @brief Returns the input definition and destination rules for Add. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Target };
        static const SOperationDefinition Definition{
            EParticleOp::Add, Inputs, false,
            false, true
        };
        return Definition;
    }
}
