#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::LerpOverLife
{
    namespace Inputs
    {
        /** @brief Position of the Start argument for LerpOverLife. */
        inline constexpr uint32_t Start = 0;

        /** @brief Position of the End argument for LerpOverLife. */
        inline constexpr uint32_t End = 1;
    }

    /** @brief Returns the input definition and destination rules for LerpOverLife. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{
            EOperandType::Target,
            EOperandType::Target
        };
        static constexpr SOperationDefinition definition{
            EOperationType::LerpOverLife,
            inputs,
            false,
            true
        };
        return definition;
    }
}
