#pragma once

#include <Engine/Core/Core.h>

namespace Elixir
{
    /**
     * @brief Identifies a pooled resource while detecting stale references.
     * @tparam T Resource type identified by this handle.
     */
    template <typename T>
    struct SHandle
    {
        /** Slot occupied by the resource in its pool. */
        uint32_t Index = std::numeric_limits<uint32_t>::max();

        /** Version of the slot when this handle was created. */
        uint32_t Generation = 0;

        /** @brief Check whether this handle identifies a pool slot. */
        bool IsValid() const
        {
            return Index != std::numeric_limits<uint32_t>::max();
        }
    };
}
