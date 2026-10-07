#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::AddWithDelta
{
    namespace Inputs
    {
        /** @brief Position of the Value argument for AddWithDelta. */
        inline constexpr uint32_t Value = 0;

        /** @brief Position of the Multiplier argument for AddWithDelta. */
        inline constexpr uint32_t Multiplier = 1;
    }

    /** @brief Returns the input definition and destination rules for AddWithDelta. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{
            EOperandType::Target,
            EOperandType::Scalar
        };
        static constexpr SOperationDefinition definition{
            EOperationType::AddWithDelta,
            inputs,
            false,
            true
        };
        return definition;
    }
}
