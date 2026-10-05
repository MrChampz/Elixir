#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Center argument for SampleDisk. */
    inline constexpr uint32_t SampleDisk_Center = 0;
    /** @brief Position of the Radius argument for SampleDisk. */
    inline constexpr uint32_t SampleDisk_Radius = 1;
    /** @brief Position of the Normal argument for SampleDisk. */
    inline constexpr uint32_t SampleDisk_Normal = 2;
}

namespace Elixir::Aether::Core::Operations::SampleDisk
{
    /** @brief Returns the input definition and destination rules for SampleDisk. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Vector3, EOperandType::Scalar, EOperandType::Vector3 };
        static const SOperationDefinition Definition{
            EParticleOp::SampleDisk, Inputs, false,
            true, false
        };
        return Definition;
    }
}
