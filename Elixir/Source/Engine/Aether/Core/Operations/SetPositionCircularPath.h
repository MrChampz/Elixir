#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for SetPositionCircularPath. */
    class SetPositionCircularPath final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for SetPositionCircularPath. */
        struct Inputs
        {
            /** @brief Position of the BaseOffset argument for SetPositionCircularPath. */
            static constexpr uint32_t BaseOffset = 0;

            /** @brief Position of the TimeScale argument for SetPositionCircularPath. */
            static constexpr uint32_t TimeScale = 1;

            /** @brief Position of the PrimaryAmplitude argument for SetPositionCircularPath. */
            static constexpr uint32_t PrimaryAmplitude = 2;

            /** @brief Position of the SecondaryAmplitude argument for SetPositionCircularPath. */
            static constexpr uint32_t SecondaryAmplitude = 3;
        };

        /** @brief Returns the input definition and destination rules for SetPositionCircularPath. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandInput::Float3,
                EOperandInput::Float,
                EOperandInput::Float3,
                EOperandInput::Float3
            };
            static constexpr SOperationDefinition definition{
                EOperationType::SetPositionCircularPath,
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
