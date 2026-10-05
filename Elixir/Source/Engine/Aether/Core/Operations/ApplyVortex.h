#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Center argument for ApplyVortex. */
    inline constexpr uint32_t ApplyVortex_Center = 0;
    /** @brief Position of the Normal argument for ApplyVortex. */
    inline constexpr uint32_t ApplyVortex_Normal = 1;
    /** @brief Position of the Tangential argument for ApplyVortex. */
    inline constexpr uint32_t ApplyVortex_Tangential = 2;
    /** @brief Position of the Radial argument for ApplyVortex. */
    inline constexpr uint32_t ApplyVortex_Radial = 3;
}

namespace Elixir::Aether::Core::Operations::ApplyVortex
{
    /** @brief Returns the input definition and destination rules for ApplyVortex. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Vector3, EOperandType::Vector3, EOperandType::Scalar, EOperandType::Scalar };
        static const SOperationDefinition Definition{
            EParticleOp::ApplyVortex, Inputs, false,
            false, true
        };
        return Definition;
    }
}
