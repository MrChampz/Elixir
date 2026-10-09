#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for SetPositionOnCircle. */
    class SetPositionOnCircle final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for SetPositionOnCircle. */
        struct Inputs
        {
            /** @brief Position of the Center argument for SetPositionOnCircle. */
            static constexpr uint32_t Center = 0;

            /** @brief Position of the Radius argument for SetPositionOnCircle. */
            static constexpr uint32_t Radius = 1;

            /** @brief Position of the AngularSpeed argument for SetPositionOnCircle. */
            static constexpr uint32_t AngularSpeed = 2;

            /** @brief Position of the StartAngle argument for SetPositionOnCircle. */
            static constexpr uint32_t StartAngle = 3;
        };

        /** @brief Returns the input definition and destination rules for SetPositionOnCircle. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandInput::Float3,
                EOperandInput::Float,
                EOperandInput::Float,
                EOperandInput::Float
            };
            static constexpr SOperationDefinition definition{
                EOperationType::SetPositionOnCircle,
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
