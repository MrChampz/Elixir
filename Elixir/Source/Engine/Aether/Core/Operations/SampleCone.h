#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for SampleCone. */
    class SampleCone final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for SampleCone. */
        struct Inputs
        {
            /** @brief Position of the Direction argument for SampleCone. */
            static constexpr uint32_t Direction = 0;

            /** @brief Position of the Angle argument for SampleCone. */
            static constexpr uint32_t Angle = 1;

            /** @brief Position of the MinSpeed argument for SampleCone. */
            static constexpr uint32_t MinSpeed = 2;

            /** @brief Position of the MaxSpeed argument for SampleCone. */
            static constexpr uint32_t MaxSpeed = 3;
        };

        /** @brief Returns the input definition and destination rules for SampleCone. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandType::Vector3,
                EOperandType::Scalar,
                EOperandType::Scalar,
                EOperandType::Scalar
            };
            static constexpr SOperationDefinition definition{
                EOperationType::SampleCone,
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
