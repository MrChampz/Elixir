#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Start argument for LerpOverLife. */
    inline constexpr uint32_t LerpOverLife_Start = 0;
    /** @brief Position of the End argument for LerpOverLife. */
    inline constexpr uint32_t LerpOverLife_End = 1;
}

namespace Elixir::Aether::Core::Operations::LerpOverLife
{
    /** @brief Returns the input definition and destination rules for LerpOverLife. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Target, EOperandType::Target };
        static const SOperationDefinition Definition{
            EParticleOp::LerpOverLife, Inputs, false,
            false, true
        };
        return Definition;
    }
}
