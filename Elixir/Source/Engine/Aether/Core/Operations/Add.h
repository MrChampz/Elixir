#pragma once

#include <Engine/Aether/Core/Operation.h>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for Add. */
    class Add final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for Add. */
        struct Inputs
        {
            /** @brief Position of the Value argument for Add. */
            static constexpr uint32_t Value = 0;
        };

        /** @brief Returns the input definition and destination rules for Add. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{ EOperandType::Target };
            static constexpr SOperationDefinition definition{
                EOperationType::Add,
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
