#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the curl-force operands and velocity destination. */
    class ApplyCurlNoise final : public Operation
    {
    public:
        /** @brief Returns strength, frequency, scroll velocity, and native seed inputs. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{EOperandInput::Float, EOperandInput::Float, EOperandInput::Float3, EOperandInput::UInt};
            static constexpr SOperationDefinition definition{EOperationType::ApplyCurlNoise, inputs, false, true};
            return definition;
        }

        /** @brief Curl forces update velocity only. */
        bool IsValidTarget(EParticleAttribute target) const override { return target == EParticleAttribute::Velocity; }
    };
}
