#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::Clamp
{
    namespace Inputs
    {
        /** @brief Position of the Minimum argument for Clamp. */
        inline constexpr uint32_t Minimum = 0;

        /** @brief Position of the Maximum argument for Clamp. */
        inline constexpr uint32_t Maximum = 1;
    }

    /** @brief Returns the input definition and destination rules for Clamp. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{
            EOperandType::Target,
            EOperandType::Target
        };
        static constexpr SOperationDefinition definition{
            EOperationType::Clamp,
            inputs,
            false,
            true
        };
        return definition;
    }
}
