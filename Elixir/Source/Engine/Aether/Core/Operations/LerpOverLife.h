#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for LerpOverLife. */
    class LerpOverLife final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for LerpOverLife. */
        struct Inputs
        {
            /** @brief Position of the Start argument for LerpOverLife. */
            static constexpr uint32_t Start = 0;

            /** @brief Position of the End argument for LerpOverLife. */
            static constexpr uint32_t End = 1;
        };

        /** @brief Returns the input definition and destination rules for LerpOverLife. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandType::Target,
                EOperandType::Target
            };
            static constexpr SOperationDefinition definition{
                EOperationType::LerpOverLife,
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
