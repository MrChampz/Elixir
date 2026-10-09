#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for SampleBox. */
    class SampleBox final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for SampleBox. */
        struct Inputs
        {
            /** @brief Position of the Minimum argument for SampleBox. */
            static constexpr uint32_t Minimum = 0;

            /** @brief Position of the Maximum argument for SampleBox. */
            static constexpr uint32_t Maximum = 1;
        };

        /** @brief Returns the input definition and destination rules for SampleBox. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandInput::Float3,
                EOperandInput::Float3
            };
            static constexpr SOperationDefinition definition{
                EOperationType::SampleBox,
                inputs,
                true,
                false
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
