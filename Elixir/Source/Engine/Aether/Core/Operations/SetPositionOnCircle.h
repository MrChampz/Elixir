#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Center argument for SetPositionOnCircle. */
    inline constexpr uint32_t SetPositionOnCircle_Center = 0;
    /** @brief Position of the Radius argument for SetPositionOnCircle. */
    inline constexpr uint32_t SetPositionOnCircle_Radius = 1;
    /** @brief Position of the AngularSpeed argument for SetPositionOnCircle. */
    inline constexpr uint32_t SetPositionOnCircle_AngularSpeed = 2;
    /** @brief Position of the StartAngle argument for SetPositionOnCircle. */
    inline constexpr uint32_t SetPositionOnCircle_StartAngle = 3;
}

namespace Elixir::Aether::Core::Operations::SetPositionOnCircle
{
    /** @brief Returns the input definition and destination rules for SetPositionOnCircle. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Vector3, EOperandType::Scalar, EOperandType::Scalar, EOperandType::Scalar };
        static const SOperationDefinition Definition{
            EParticleOp::SetPositionOnCircle, Inputs, false,
            true, false
        };
        return Definition;
    }
}
