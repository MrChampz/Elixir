#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the BaseOffset argument for SetPositionCircularPath. */
    inline constexpr uint32_t SetPositionCircularPath_BaseOffset = 0;
    /** @brief Position of the TimeScale argument for SetPositionCircularPath. */
    inline constexpr uint32_t SetPositionCircularPath_TimeScale = 1;
    /** @brief Position of the PrimaryAmplitude argument for SetPositionCircularPath. */
    inline constexpr uint32_t SetPositionCircularPath_PrimaryAmplitude = 2;
    /** @brief Position of the SecondaryAmplitude argument for SetPositionCircularPath. */
    inline constexpr uint32_t SetPositionCircularPath_SecondaryAmplitude = 3;
}

namespace Elixir::Aether::Core::Operations::SetPositionCircularPath
{
    /** @brief Returns the input definition and destination rules for SetPositionCircularPath. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Vector3, EOperandType::Scalar, EOperandType::Vector3, EOperandType::Vector3 };
        static const SOperationDefinition Definition{
            EParticleOp::SetPositionCircularPath, Inputs, false,
            true, false
        };
        return Definition;
    }
}
