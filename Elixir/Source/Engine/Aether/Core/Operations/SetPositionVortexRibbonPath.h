#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Center argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_Center = 0;
    /** @brief Position of the OrbitSpeed argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_OrbitSpeed = 1;
    /** @brief Position of the BaseRadius argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_BaseRadius = 2;
    /** @brief Position of the RadiusAmplitude argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_RadiusAmplitude = 3;
    /** @brief Position of the RadiusSpeed argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_RadiusSpeed = 4;
    /** @brief Position of the PulseAmplitude argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_PulseAmplitude = 5;
    /** @brief Position of the PulseSpeed argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_PulseSpeed = 6;
    /** @brief Position of the CurlAmplitude argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_CurlAmplitude = 7;
    /** @brief Position of the DepthAmplitude argument for SetPositionVortexRibbonPath. */
    inline constexpr uint32_t SetPositionVortexRibbonPath_DepthAmplitude = 8;
}

namespace Elixir::Aether::Core::Operations::SetPositionVortexRibbonPath
{
    /** @brief Returns the input definition and destination rules for SetPositionVortexRibbonPath. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Vector3, EOperandType::Scalar, EOperandType::Scalar, EOperandType::Scalar, EOperandType::Scalar, EOperandType::Scalar, EOperandType::Scalar, EOperandType::Scalar, EOperandType::Scalar };
        static const SOperationDefinition Definition{
            EParticleOp::SetPositionVortexRibbonPath, Inputs, false,
            true, false
        };
        return Definition;
    }
}
