#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::Dampen
{
    namespace Inputs
    {
        /** @brief Position of the Drag argument for Dampen. */
        inline constexpr uint32_t Drag = 0;
    }

    /** @brief Returns the input definition and destination rules for Dampen. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{ EOperandType::Scalar };
        static constexpr SOperationDefinition definition{
            EOperationType::Dampen,
            inputs,
            false,
            true
        };
        return definition;
    }
}
