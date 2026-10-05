#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Value argument for AddFromAttribute. */
    inline constexpr uint32_t AddFromAttribute_Value = 0;
    /** @brief Position of the Scale argument for AddFromAttribute. */
    inline constexpr uint32_t AddFromAttribute_Scale = 1;
}

namespace Elixir::Aether::Core::Operations::AddFromAttribute
{
    /** @brief Returns the input definition and destination rules for AddFromAttribute. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Target, EOperandType::Scalar };
        static const SOperationDefinition Definition{
            EParticleOp::AddFromAttribute, Inputs, false,
            false, true
        };
        return Definition;
    }
}
