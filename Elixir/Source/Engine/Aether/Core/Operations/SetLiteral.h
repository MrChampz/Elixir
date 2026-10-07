#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::SetLiteral
{
    namespace Inputs
    {
        /** @brief Position of the Value argument for SetLiteral. */
        inline constexpr uint32_t Value = 0;
    }

    /** @brief Returns the input definition and destination rules for SetLiteral. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{ EOperandType::Target };
        static constexpr SOperationDefinition definition{
            EOperationType::SetLiteral,
            inputs,
            true,
            true
        };
        return definition;
    }
}
