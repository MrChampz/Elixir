#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for SetRibbonIdFromSpawnOrder. */
    class SetRibbonIdFromSpawnOrder final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for SetRibbonIdFromSpawnOrder. */
        struct Inputs
        {
            /** @brief Position of the RibbonCount argument for SetRibbonIdFromSpawnOrder. */
            static constexpr uint32_t RibbonCount = 0;

            /** @brief Position of the FirstRibbonId argument for SetRibbonIdFromSpawnOrder. */
            static constexpr uint32_t FirstRibbonId = 1;
        };

        /** @brief Returns the input definition and destination rules for SetRibbonIdFromSpawnOrder. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandInput::UInt,
                EOperandInput::UInt
            };
            static constexpr SOperationDefinition definition{
                EOperationType::SetRibbonIdFromSpawnOrder,
                inputs,
                true,
                false
            };
            return definition;
        }

        /** @brief Checks whether this operation supports the destination attribute. */
        bool IsValidTarget(const EParticleAttribute target) const override
        {
            return target == EParticleAttribute::RibbonId;
        }
    };
}
