#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for RandomRange. */
    class RandomRange final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for RandomRange. */
        struct Inputs
        {
            /** @brief Position of the Minimum argument for RandomRange. */
            static constexpr uint32_t Minimum = 0;

            /** @brief Position of the Maximum argument for RandomRange. */
            static constexpr uint32_t Maximum = 1;
        };

        /** @brief Returns the input definition and destination rules for RandomRange. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandInput::Target,
                EOperandInput::Target
            };
            static constexpr SOperationDefinition definition{
                EOperationType::RandomRange,
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
