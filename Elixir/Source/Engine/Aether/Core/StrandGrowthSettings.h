#pragma once
#include <cstdint>
#include <cmath>

namespace Elixir::Aether::Core
{
    /** Selects the strand development state independently of physical motion. */
    enum class EStrandGrowthState : uint32_t { Growing, Mature, Paused };

    /** Configures strand development and branching. */
    struct SStrandGrowthSettings
    {
        /** Tip speed in simulation units per second; zero starts fully grown. */
        float Speed = 0;
        /** Maximum main-chain length; zero uses the point capacity. */
        float MaxLength = 0;
        /** Side-chain capacity per main chain, from zero to four. */
        uint32_t BranchCount = 0;
        /** Nominal interval between branch nodes, in control segments. */
        uint32_t BranchInterval = 8;
        /** Side-chain length and speed multiplier, greater than zero and at most one. */
        float BranchLengthScale = 0.5f;
        /** Fraction of half the branch interval used for seeded node jitter, from zero to one. */
        float BranchVariation = 0;
        /** Maximum extra seeded delay before each branch grows, in seconds. */
        float BranchDelay = 0;
        /** Reproducible seed for branch location and timing variation. */
        uint32_t Seed = 0;
        /** Initial development state; pausing does not pause the solver. */
        EStrandGrowthState State = EStrandGrowthState::Growing;

        /** Checks authoring ranges before compiling development data. */
        bool IsValid() const
        {
            return std::isfinite(Speed) && Speed >= 0 && std::isfinite(MaxLength) && MaxLength >= 0 &&
                BranchCount <= 4 && BranchInterval >= 1 && BranchInterval <= 96 &&
                std::isfinite(BranchLengthScale) && BranchLengthScale > 0 && BranchLengthScale <= 1 &&
                std::isfinite(BranchVariation) && BranchVariation >= 0 && BranchVariation <= 1 &&
                std::isfinite(BranchDelay) && BranchDelay >= 0 && uint32_t(State) <= uint32_t(EStrandGrowthState::Paused);
        }
    };
}
