#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Direction argument for SampleCone. */
    inline constexpr uint32_t SampleCone_Direction = 0;
    /** @brief Position of the Angle argument for SampleCone. */
    inline constexpr uint32_t SampleCone_Angle = 1;
    /** @brief Position of the MinSpeed argument for SampleCone. */
    inline constexpr uint32_t SampleCone_MinSpeed = 2;
    /** @brief Position of the MaxSpeed argument for SampleCone. */
    inline constexpr uint32_t SampleCone_MaxSpeed = 3;
}

namespace Elixir::Aether::Core::Operations::SampleCone
{
    /** @brief Returns the input definition and destination rules for SampleCone. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Vector3, EOperandType::Scalar, EOperandType::Scalar, EOperandType::Scalar };
        static const SOperationDefinition Definition{
            EParticleOp::SampleCone, Inputs, false,
            true, false
        };
        return Definition;
    }
}
