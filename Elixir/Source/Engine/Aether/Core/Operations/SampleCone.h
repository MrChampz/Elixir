#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::SampleCone
{
    namespace Inputs
    {
        /** @brief Position of the Direction argument for SampleCone. */
        inline constexpr uint32_t Direction = 0;

        /** @brief Position of the Angle argument for SampleCone. */
        inline constexpr uint32_t Angle = 1;

        /** @brief Position of the MinSpeed argument for SampleCone. */
        inline constexpr uint32_t MinSpeed = 2;

        /** @brief Position of the MaxSpeed argument for SampleCone. */
        inline constexpr uint32_t MaxSpeed = 3;
    }

    /** @brief Returns the input definition and destination rules for SampleCone. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{
            EOperandType::Vector3,
            EOperandType::Scalar,
            EOperandType::Scalar,
            EOperandType::Scalar
        };
        static constexpr SOperationDefinition definition{
            EOperationType::SampleCone,
            inputs,
            true,
            false
        };
        return definition;
    }
}
