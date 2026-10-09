#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for Dampen. */
    class Dampen final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for Dampen. */
        struct Inputs
        {
            /** @brief Position of the Drag argument for Dampen. */
            static constexpr uint32_t Drag = 0;
        };

        /** @brief Returns the input definition and destination rules for Dampen. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{ EOperandInput::Float };
            static constexpr SOperationDefinition definition{
                EOperationType::Dampen,
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
