#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::SampleColorCurve
{
    namespace Inputs
    {
        /** @brief Position of the Curve argument for SampleColorCurve. */
        inline constexpr uint32_t Curve = 0;

        /** @brief Position of the Input argument for SampleColorCurve. */
        inline constexpr uint32_t Input = 1;
    }

    /** @brief Returns the input definition and destination rules for SampleColorCurve. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{
            EOperandType::Curve,
            EOperandType::Scalar
        };
        static constexpr SOperationDefinition definition{
            EOperationType::SampleColorCurve,
            inputs,
            false,
            true
        };
        return definition;
    }
}
