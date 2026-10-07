#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::Mul
{
    namespace Inputs
    {
        /** @brief Position of the Value argument for Mul. */
        inline constexpr uint32_t Value = 0;
    }

    /** @brief Returns the input definition and destination rules for Mul. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{ EOperandType::Target };
        static constexpr SOperationDefinition definition{
            EOperationType::Mul,
            inputs,
            false,
            true
        };
        return definition;
    }
}
