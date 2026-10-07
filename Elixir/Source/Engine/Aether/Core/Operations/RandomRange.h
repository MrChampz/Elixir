#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::RandomRange
{
    namespace Inputs
    {
        /** @brief Position of the Minimum argument for RandomRange. */
        inline constexpr uint32_t Minimum = 0;

        /** @brief Position of the Maximum argument for RandomRange. */
        inline constexpr uint32_t Maximum = 1;
    }

    /** @brief Returns the input definition and destination rules for RandomRange. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{
            EOperandType::Target,
            EOperandType::Target
        };
        static constexpr SOperationDefinition definition{
            EOperationType::RandomRange,
            inputs,
            true,
            false
        };
        return definition;
    }
}
