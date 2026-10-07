#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <array>

namespace Elixir::Aether::Core::Operations
{
    /** @brief Defines the arguments and allowed destinations for SetPositionVortexRibbonPath. */
    class SetPositionVortexRibbonPath final : public Operation
    {
    public:
        /** @brief Identifies the argument positions for SetPositionVortexRibbonPath. */
        struct Inputs
        {
            /** @brief Position of the Center argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t Center = 0;

            /** @brief Position of the OrbitSpeed argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t OrbitSpeed = 1;

            /** @brief Position of the BaseRadius argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t BaseRadius = 2;

            /** @brief Position of the RadiusAmplitude argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t RadiusAmplitude = 3;

            /** @brief Position of the RadiusSpeed argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t RadiusSpeed = 4;

            /** @brief Position of the PulseAmplitude argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t PulseAmplitude = 5;

            /** @brief Position of the PulseSpeed argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t PulseSpeed = 6;

            /** @brief Position of the CurlAmplitude argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t CurlAmplitude = 7;

            /** @brief Position of the DepthAmplitude argument for SetPositionVortexRibbonPath. */
            static constexpr uint32_t DepthAmplitude = 8;
        };

        /** @brief Returns the input definition and destination rules for SetPositionVortexRibbonPath. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandType::Vector3,
                EOperandType::Scalar,
                EOperandType::Scalar,
                EOperandType::Scalar,
                EOperandType::Scalar,
                EOperandType::Scalar,
                EOperandType::Scalar,
                EOperandType::Scalar,
                EOperandType::Scalar
            };
            static constexpr SOperationDefinition definition{
                EOperationType::SetPositionVortexRibbonPath,
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
