#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for ApplyVortex. */
    class ApplyVortex final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for ApplyVortex. */
        struct Inputs
        {
            /** @brief Position of the Center argument for ApplyVortex. */
            static constexpr uint32_t Center = 0;

            /** @brief Position of the Normal argument for ApplyVortex. */
            static constexpr uint32_t Normal = 1;

            /** @brief Position of the Tangential argument for ApplyVortex. */
            static constexpr uint32_t Tangential = 2;

            /** @brief Position of the Radial argument for ApplyVortex. */
            static constexpr uint32_t Radial = 3;
        };

        /** @brief Returns the input definition and destination rules for ApplyVortex. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandType::Vector3,
                EOperandType::Vector3,
                EOperandType::Scalar,
                EOperandType::Scalar
            };
            static constexpr SOperationDefinition definition{
                EOperationType::ApplyVortex,
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
