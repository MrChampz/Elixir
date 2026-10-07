#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::SampleDisk
{
    namespace Inputs
    {
        /** @brief Position of the Center argument for SampleDisk. */
        inline constexpr uint32_t Center = 0;

        /** @brief Position of the Radius argument for SampleDisk. */
        inline constexpr uint32_t Radius = 1;

        /** @brief Position of the Normal argument for SampleDisk. */
        inline constexpr uint32_t Normal = 2;
    }

    /** @brief Returns the input definition and destination rules for SampleDisk. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{
            EOperandType::Vector3,
            EOperandType::Scalar,
            EOperandType::Vector3
        };
        static constexpr SOperationDefinition definition{
            EOperationType::SampleDisk,
            inputs,
            true,
            false
        };
        return definition;
    }
}
