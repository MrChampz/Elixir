#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Minimum argument for KillOutsideBounds. */
    inline constexpr uint32_t KillOutsideBounds_Minimum = 0;
    /** @brief Position of the Maximum argument for KillOutsideBounds. */
    inline constexpr uint32_t KillOutsideBounds_Maximum = 1;
}

namespace Elixir::Aether::Core::Operations::KillOutsideBounds
{
    /** @brief Returns the input definition and destination rules for KillOutsideBounds. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Vector3, EOperandType::Vector3 };
        static const SOperationDefinition Definition{
            EParticleOp::KillOutsideBounds, Inputs, false,
            false, true
        };
        return Definition;
    }
}
