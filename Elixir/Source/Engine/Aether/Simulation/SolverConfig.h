#pragma once

#include <cstdint>

namespace Elixir::Aether::Simulation
{
    /** Controls execution of all constraint modules owned by an emitter. */
    struct SSolverConfig
    {
        /** Number of constraint passes per simulation step, from 1 to 16. */
        uint32_t Iterations = 8;

        /** @brief Reports whether the iteration count is supported. */
        bool IsValid() const { return Iterations >= 1 && Iterations <= 16; }
    };
}
