#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::Add
{
    namespace Inputs
    {
        /** @brief Position of the Value argument for Add. */
        inline constexpr uint32_t Value = 0;
    }

    /** @brief Returns the input definition and destination rules for Add. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{ EOperandType::Target };
        static constexpr SOperationDefinition definition{
            EOperationType::Add,
            inputs,
            false,
            true
        };
        return definition;
    }
}
