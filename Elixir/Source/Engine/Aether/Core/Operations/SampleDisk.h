#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for SampleDisk. */
    class SampleDisk final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for SampleDisk. */
        struct Inputs
        {
            /** @brief Position of the Center argument for SampleDisk. */
            static constexpr uint32_t Center = 0;

            /** @brief Position of the Radius argument for SampleDisk. */
            static constexpr uint32_t Radius = 1;

            /** @brief Position of the Normal argument for SampleDisk. */
            static constexpr uint32_t Normal = 2;
        };

        /** @brief Returns the input definition and destination rules for SampleDisk. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandType::Vector3,
                EOperandType::Scalar,
                EOperandType::Vector3
            };
            static constexpr SOperationDefinition definition{
                EOperationType::SampleDisk,
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
