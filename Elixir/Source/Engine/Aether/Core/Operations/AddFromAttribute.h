#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::AddFromAttribute
{
    namespace Inputs
    {
        /** @brief Position of the Value argument for AddFromAttribute. */
        inline constexpr uint32_t Value = 0;

        /** @brief Position of the Scale argument for AddFromAttribute. */
        inline constexpr uint32_t Scale = 1;
    }

    /** @brief Returns the input definition and destination rules for AddFromAttribute. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{
            EOperandType::Target,
            EOperandType::Scalar
        };
        static constexpr SOperationDefinition definition{
            EOperationType::AddFromAttribute,
            inputs,
            false,
            true
        };
        return definition;
    }
}
