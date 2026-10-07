#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::ApplyVortex
{
    namespace Inputs
    {
        /** @brief Position of the Center argument for ApplyVortex. */
        inline constexpr uint32_t Center = 0;

        /** @brief Position of the Normal argument for ApplyVortex. */
        inline constexpr uint32_t Normal = 1;

        /** @brief Position of the Tangential argument for ApplyVortex. */
        inline constexpr uint32_t Tangential = 2;

        /** @brief Position of the Radial argument for ApplyVortex. */
        inline constexpr uint32_t Radial = 3;
    }

    /** @brief Returns the input definition and destination rules for ApplyVortex. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{
            EOperandType::Vector3,
            EOperandType::Vector3,
            EOperandType::Scalar,
            EOperandType::Scalar
        };
        static constexpr SOperationDefinition definition{
            EOperationType::ApplyVortex,
            inputs,
            false,
            true
        };
        return definition;
    }
}
