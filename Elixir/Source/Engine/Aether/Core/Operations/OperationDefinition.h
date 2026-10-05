#pragma once

#include <Engine/Aether/Core/Operand.h>
#include <Engine/Aether/Core/Particle.h>
#include <span>

namespace Elixir::Aether::Core
{
    /** @brief Describes the inputs and destination rules of a particle operation. */
    struct SOperationDefinition
    {
        EParticleOp Type;

        /** @brief Required logical type at each argument position. */
        std::span<const EOperandType> Inputs;

        /** @brief Allows an unsigned integer when the destination is RibbonId. */
        bool IntegerRibbonTarget;

        /** @brief Indicates whether the spawn shader executes this operation. */
        bool SupportsSpawn;

        /** @brief Indicates whether the update shader executes this operation. */
        bool SupportsUpdate;
    };
}
