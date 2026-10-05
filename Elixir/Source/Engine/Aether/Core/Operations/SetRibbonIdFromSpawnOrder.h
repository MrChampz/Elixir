#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Inputs
{
    /** @brief Position of the RibbonCount argument for SetRibbonIdFromSpawnOrder. */
    inline constexpr uint32_t SetRibbonIdFromSpawnOrder_RibbonCount = 0;
    /** @brief Position of the FirstRibbonId argument for SetRibbonIdFromSpawnOrder. */
    inline constexpr uint32_t SetRibbonIdFromSpawnOrder_FirstRibbonId = 1;
}

namespace Elixir::Aether::Core::Operations::SetRibbonIdFromSpawnOrder
{
    /** @brief Returns the input definition and destination rules for SetRibbonIdFromSpawnOrder. */
    inline const SOperationDefinition& GetDefinition()
    {
        static const std::array Inputs{ EOperandType::UInt, EOperandType::UInt };
        static const SOperationDefinition Definition{
            EParticleOp::SetRibbonIdFromSpawnOrder, Inputs, false,
            true, false
        };
        return Definition;
    }
}
