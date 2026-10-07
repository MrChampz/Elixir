#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for CopyFromAttribute. */
    class CopyFromAttribute final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for CopyFromAttribute. */
        struct Inputs
        {
            /** @brief Position of the Value argument for CopyFromAttribute. */
            static constexpr uint32_t Value = 0;
        };

        /** @brief Returns the input definition and destination rules for CopyFromAttribute. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{ EOperandType::Target };
            static constexpr SOperationDefinition definition{
                EOperationType::CopyFromAttribute,
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
