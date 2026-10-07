#pragma once

#include "OperationDefinition.h"
#include <array>

namespace Elixir::Aether::Core::Operations::SetRibbonIdFromSpawnOrder
{
    namespace Inputs
    {
        /** @brief Position of the RibbonCount argument for SetRibbonIdFromSpawnOrder. */
        inline constexpr uint32_t RibbonCount = 0;

        /** @brief Position of the FirstRibbonId argument for SetRibbonIdFromSpawnOrder. */
        inline constexpr uint32_t FirstRibbonId = 1;
    }

    /** @brief Returns the input definition and destination rules for SetRibbonIdFromSpawnOrder. */
    inline const SOperationDefinition& GetDefinition()
    {
        static constexpr std::array inputs{
            EOperandType::UInt,
            EOperandType::UInt
        };
        static constexpr SOperationDefinition definition{
            EOperationType::SetRibbonIdFromSpawnOrder,
            inputs,
            true,
            false
        };
        return definition;
    }
}
