#pragma once

#include <Engine/Aether/Core/Operation.h>
#include <Engine/Aether/Core/Operand.h>

namespace Elixir::Aether::Core
{
    /** @brief Describes the inputs and destination rules of a particle operation. */
    struct SOperationDefinition
    {
        EOperationType Type;

        /** @brief Required logical type at each argument position. */
        std::span<const EOperandType> Inputs;

        /** @brief Indicates whether the spawn shader executes this operation. */
        bool SupportsSpawn;

        /** @brief Indicates whether the update shader executes this operation. */
        bool SupportsUpdate;
    };
}
