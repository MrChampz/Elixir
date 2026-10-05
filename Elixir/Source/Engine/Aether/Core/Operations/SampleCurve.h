#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the Curve argument for SampleCurve. */
    inline constexpr uint32_t SampleCurve_Curve = 0;
    /** @brief Position of the Input argument for SampleCurve. */
    inline constexpr uint32_t SampleCurve_Input = 1;
}

namespace Elixir::Aether::Core::Operations::SampleCurve
{
    /** @brief Returns the input definition and destination rules for SampleCurve. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::Curve, EOperandType::Scalar };
        static const SOperationDefinition Definition{
            EParticleOp::SampleCurve, Inputs, false,
            true, true
        };
        return Definition;
    }
}
