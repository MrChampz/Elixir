#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /**
     * @brief Defines the arguments and allowed destinations for SetPositionVortexRibbonPath.
     */
    class SetPositionVortexRibbonPath final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for SetPositionVortexRibbonPath. */
        struct Inputs
        {
            /** Position of the Center argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t Center = 0;

            /** Position of the OrbitSpeed argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t OrbitSpeed = 1;

            /** Position of the BaseRadius argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t BaseRadius = 2;

            /** Position of the RadiusAmplitude argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t RadiusAmplitude = 3;

            /** Position of the RadiusSpeed argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t RadiusSpeed = 4;

            /** Position of the PulseAmplitude argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t PulseAmplitude = 5;

            /** Position of the PulseSpeed argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t PulseSpeed = 6;

            /** Position of the CurlAmplitude argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t CurlAmplitude = 7;

            /** Position of the DepthAmplitude argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t DepthAmplitude = 8;
        };

        /**
         * @brief Get the input definition and destination rules for the vortex ribbon path.
         */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandInput::Float3,
                EOperandInput::Float,
                EOperandInput::Float,
                EOperandInput::Float,
                EOperandInput::Float,
                EOperandInput::Float,
                EOperandInput::Float,
                EOperandInput::Float,
                EOperandInput::Float
            };
            static constexpr SOperationDefinition definition{
                EOperationType::SetPositionVortexRibbonPath,
                inputs,
                true,
                false
            };
            return definition;
        }

        /**
         * @brief Check whether the vortex ribbon path supports a destination attribute.
         * @param target Particle attribute that the operation would write.
         */
        bool IsValidTarget(const EParticleAttribute target) const override
        {
            return target <= EParticleAttribute::Temp3 &&
                target != EParticleAttribute::RibbonId;
        }
    };
}
