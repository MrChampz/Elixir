#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Value argument for CopyFromAttribute. */
    inline constexpr uint32_t CopyFromAttribute_Value = 0;
}

namespace Elixir::Aether::Core::Operations::CopyFromAttribute
{
    /** @brief Returns the input definition and destination rules for CopyFromAttribute. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Target };
        static const SOperationDefinition Definition{
            EParticleOp::CopyFromAttribute, Inputs, false,
            true, true
        };
        return Definition;
    }
}
