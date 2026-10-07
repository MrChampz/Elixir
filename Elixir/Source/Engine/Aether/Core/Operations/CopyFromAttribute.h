#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::CopyFromAttribute
{
    namespace Inputs
    {
        /** @brief Position of the Value argument for CopyFromAttribute. */
        inline constexpr uint32_t Value = 0;
    }

    /** @brief Returns the input definition and destination rules for CopyFromAttribute. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{ EOperandType::Target };
        static constexpr SOperationDefinition definition{
            EOperationType::CopyFromAttribute,
            inputs,
            true,
            true
        };
        return definition;
    }
}
