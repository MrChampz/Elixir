#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::SetPositionCircularPath
{
    namespace Inputs
    {
        /** @brief Position of the BaseOffset argument for SetPositionCircularPath. */
        inline constexpr uint32_t BaseOffset = 0;

        /** @brief Position of the TimeScale argument for SetPositionCircularPath. */
        inline constexpr uint32_t TimeScale = 1;

        /** @brief Position of the PrimaryAmplitude argument for SetPositionCircularPath. */
        inline constexpr uint32_t PrimaryAmplitude = 2;

        /** @brief Position of the SecondaryAmplitude argument for SetPositionCircularPath. */
        inline constexpr uint32_t SecondaryAmplitude = 3;
    }

    /** @brief Returns the input definition and destination rules for SetPositionCircularPath. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{
            EOperandType::Vector3,
            EOperandType::Scalar,
            EOperandType::Vector3,
            EOperandType::Vector3
        };
        static constexpr SOperationDefinition definition{
            EOperationType::SetPositionCircularPath,
            inputs,
            true,
            false
        };
        return definition;
    }
}
