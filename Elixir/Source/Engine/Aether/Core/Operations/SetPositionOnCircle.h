#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::SetPositionOnCircle
{
    namespace Inputs
    {
        /** @brief Position of the Center argument for SetPositionOnCircle. */
        inline constexpr uint32_t Center = 0;

        /** @brief Position of the Radius argument for SetPositionOnCircle. */
        inline constexpr uint32_t Radius = 1;

        /** @brief Position of the AngularSpeed argument for SetPositionOnCircle. */
        inline constexpr uint32_t AngularSpeed = 2;

        /** @brief Position of the StartAngle argument for SetPositionOnCircle. */
        inline constexpr uint32_t StartAngle = 3;
    }

    /** @brief Returns the input definition and destination rules for SetPositionOnCircle. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{
            EOperandType::Vector3,
            EOperandType::Scalar,
            EOperandType::Scalar,
            EOperandType::Scalar
        };
        static constexpr SOperationDefinition definition{
            EOperationType::SetPositionOnCircle,
            inputs,
            true,
            false
        };
        return definition;
    }
}
