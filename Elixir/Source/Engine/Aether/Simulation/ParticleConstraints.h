#pragma once

#include <Engine/Aether/Core/StrandGrowthSettings.h>
#include <Engine/Aether/Core/StrandEndAttachment.h>
#include <Engine/Aether/Core/StrandSurfaceSettings.h>

namespace Elixir::Aether::Simulation
{
    /**
     * Describes distance constraints emitted by a DistanceConstraint module.
     *
     * Spawn order maps directly to slots in the emitter's particle ring. Connections
     * require live particles with matching EmitterIndex and GroupId. LinkOrder must
     * advance by OrderStep. A new chain starts every MaxPoints orders within each
     * interleaved sequence. Dead particles and order gaps break connections.
     */
    struct SDistanceConstraint
    {
        /** Maximum particles per chain, from 2 to 96. */
        uint32_t MaxPoints = 24;

        /** LinkOrder difference and ring-slot stride between connected particles. */
        uint32_t OrderStep = 1;

        /** Holds each chain's first particle at its position before integration. */
        bool PinFirstParticle = true;

        /** Rest distance between consecutive particles, in simulation-space units. */
        float SegmentLength = 0.04f;

        /** Fraction of distance error corrected per iteration, from 0 to 1. */
        float Stiffness = 0.85f;

        /** Enables development of the existing particle chains without extra point storage. */
        bool HasGrowth = false;

        /** Development timing and branch topology, used when HasGrowth is true. */
        Core::SStrandGrowthSettings Growth;

        /** Optional fixed destination for the last particle of each main chain. */
        Core::SStrandEndAttachment EndAttachment;

        /** Static world surfaces that guide and hold chain particles. */
        Core::SStrandSurfaceSettings SurfaceAttachment;

        /** @brief Reports whether all constraint values are valid. */
        bool IsValid() const
        {
            return MaxPoints >= 2 &&
                MaxPoints <= 96 &&
                OrderStep > 0 &&
                std::isfinite(SegmentLength) &&
                SegmentLength > 0 &&
                std::isfinite(Stiffness) &&
                Stiffness >= 0 &&
                Stiffness <= 1 && EndAttachment.IsValid() && SurfaceAttachment.IsValid() &&
                (!HasGrowth || (Growth.IsValid() &&
                    OrderStep % (Growth.BranchCount + 1) == 0 &&
                    uint64_t(Growth.BranchCount) * Growth.BranchInterval < MaxPoints - 1 &&
                    (!EndAttachment.Enabled || Growth.Speed == 0 ||
                        Growth.State == Core::EStrandGrowthState::Mature)));
        }
    };
}
