#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Curve argument for SampleColorCurve. */
    inline constexpr uint32_t SampleColorCurve_Curve = 0;
    /** @brief Position of the Input argument for SampleColorCurve. */
    inline constexpr uint32_t SampleColorCurve_Input = 1;
}

namespace Elixir::Aether::Core::Operations::SampleColorCurve
{
    /** @brief Returns the input definition and destination rules for SampleColorCurve. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Curve, EOperandType::Scalar };
        static const SOperationDefinition Definition{
            EParticleOp::SampleColorCurve, Inputs, false,
            false, true
        };
        return Definition;
    }
}
