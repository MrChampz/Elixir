#pragma once

#include <Engine/Aether/Core/Operation.h>

namespace Elixir::Aether::Core::Operations
{
    /** Defines a seeded curl-noise sample without applying forces or advancing time. */
    class SampleCurlNoise final : public Operation
    {
    public:
        /** @brief Returns the coordinates, unsigned seed, and seed offset inputs. */
        const SOperationDefinition& GetDefinition() const override
        {
            static constexpr std::array inputs{
                EOperandInput::Float3,
                EOperandInput::UInt,
                EOperandInput::Float
            };
            static constexpr SOperationDefinition definition{
                EOperationType::SampleCurlNoise,
                inputs,
                true,
                true
            };
            return definition;
        }

        /**
         * @brief Checks whether the destination can store a three-component sample.
         * @param target Attribute that receives the sampled vector.
         */
        bool IsValidTarget(const EParticleAttribute target) const override
        {
            return target == EParticleAttribute::Position ||
                target == EParticleAttribute::Velocity ||
                target == EParticleAttribute::Tangent ||
                (target >= EParticleAttribute::Temp0 && target <= EParticleAttribute::Temp3);
        }
    };
}
