#pragma once

#include <Engine/Aether/Core/Operation.h>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for SetGroupIdFromSpawnOrder. */
    class SetGroupIdFromSpawnOrder final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for SetGroupIdFromSpawnOrder. */
        struct Inputs
        {
            /** Position of the group-count argument. */
            static constexpr uint32_t GroupCount = 0;

            /** Position of the first-group identifier argument. */
            static constexpr uint32_t FirstGroupId = 1;
        };

        /** @brief Returns the input definition and allowed destination attributes. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandInput::UInt,
                EOperandInput::UInt
            };

            static constexpr SOperationDefinition definition{
                .Type = EOperationType::SetGroupIdFromSpawnOrder,
                .Inputs = inputs,
                .SupportsSpawn = true,
                .SupportsUpdate = false
            };

            return definition;
        }

        /**
         * @brief Checks whether this operation supports the destination attribute.
         * @param target Particle attribute selected as the destination.
         */
        bool IsValidTarget(const EParticleAttribute target) const override
        {
            return target == EParticleAttribute::GroupId;
        }
    };
}
