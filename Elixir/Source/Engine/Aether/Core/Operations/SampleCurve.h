#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for SampleCurve. */
    class SampleCurve final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for SampleCurve. */
        struct Inputs
        {
            /** @brief Position of the Curve argument for SampleCurve. */
            static constexpr uint32_t Curve = 0;

            /** @brief Position of the Input argument for SampleCurve. */
            static constexpr uint32_t Input = 1;
        };

        /** @brief Returns the input definition and destination rules for SampleCurve. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandInput::Curve,
                EOperandInput::Float
            };
            static constexpr SOperationDefinition definition{
                EOperationType::SampleCurve,
                inputs,
                true,
                true
            };
            return definition;
        }

        /** @brief Checks whether this operation supports the destination attribute. */
        bool IsValidTarget(const EParticleAttribute target) const override
        {
            return target <= EParticleAttribute::Temp3 &&
                target != EParticleAttribute::RibbonId;
        }
    };
}
