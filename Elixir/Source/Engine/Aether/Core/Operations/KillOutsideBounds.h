#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::KillOutsideBounds
{
    namespace Inputs
    {
        /** @brief Position of the Minimum argument for KillOutsideBounds. */
        inline constexpr uint32_t Minimum = 0;

        /** @brief Position of the Maximum argument for KillOutsideBounds. */
        inline constexpr uint32_t Maximum = 1;
    }

    /** @brief Returns the input definition and destination rules for KillOutsideBounds. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{
            EOperandType::Vector3,
            EOperandType::Vector3
        };
        static constexpr SOperationDefinition definition{
            EOperationType::KillOutsideBounds,
            inputs,
            false,
            true
        };
        return definition;
    }
}
