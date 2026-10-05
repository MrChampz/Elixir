#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Minimum argument for SampleBox. */
    inline constexpr uint32_t SampleBox_Minimum = 0;
    /** @brief Position of the Maximum argument for SampleBox. */
    inline constexpr uint32_t SampleBox_Maximum = 1;
}

namespace Elixir::Aether::Core::Operations::SampleBox
{
    /** @brief Returns the input definition and destination rules for SampleBox. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Vector3, EOperandType::Vector3 };
        static const SOperationDefinition Definition{
            EParticleOp::SampleBox, Inputs, false,
            true, false
        };
        return Definition;
    }
}
