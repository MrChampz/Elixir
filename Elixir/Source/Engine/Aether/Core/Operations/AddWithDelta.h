#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for AddWithDelta. */
    class AddWithDelta final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for AddWithDelta. */
        struct Inputs
        {
            /** @brief Position of the Value argument for AddWithDelta. */
            static constexpr uint32_t Value = 0;

            /** @brief Position of the Multiplier argument for AddWithDelta. */
            static constexpr uint32_t Multiplier = 1;
        };

        /** @brief Returns the input definition and destination rules for AddWithDelta. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandInput::Target,
                EOperandInput::Float
            };
            static constexpr SOperationDefinition definition{
                EOperationType::AddWithDelta,
                inputs,
                false,
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
