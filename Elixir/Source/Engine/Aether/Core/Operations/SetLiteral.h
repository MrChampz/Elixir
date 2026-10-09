#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for SetLiteral. */
    class SetLiteral final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for SetLiteral. */
        struct Inputs
        {
            /** @brief Position of the Value argument for SetLiteral. */
            static constexpr uint32_t Value = 0;
        };

        /** @brief Returns the input definition and destination rules for SetLiteral. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{ EOperandInput::Target };
            static constexpr SOperationDefinition definition{
                EOperationType::SetLiteral,
                inputs,
                true,
                true
            };
            return definition;
        }

        /** @brief Checks whether this operation supports the destination attribute. */
        bool IsValidTarget(const EParticleAttribute target) const override
        {
            return target <= EParticleAttribute::Temp3;
        }
    };
}
