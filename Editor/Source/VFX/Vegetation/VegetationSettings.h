#pragma once

#include "StrandGrowthSettings.h"
#include "StrandEndAttachment.h"
#include "StrandSurfaceSettings.h"

namespace EditorVFX
{
    /** Defines the Editor sample's chain layout, development, and attachments. */
    struct VegetationSettings
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
        SStrandGrowthSettings Growth;

        /** Optional fixed destination for the last particle of each main chain. */
        SStrandEndAttachment EndAttachment;

        /** Static world surfaces that guide and hold chain particles. */
        SStrandSurfaceSettings SurfaceAttachment;

        /** Constraint iterations used by the application solver. */
        uint32_t SolverIterations = 8;

        /** @brief Reports whether all vegetation settings are valid. */
        bool IsValid() const
        {
            return SolverIterations >= 1 && SolverIterations <= 16 && MaxPoints >= 2 &&
                   MaxPoints <= 96 && OrderStep > 0 && std::isfinite(SegmentLength) &&
                   SegmentLength > 0 && std::isfinite(Stiffness) && Stiffness >= 0 &&
                   Stiffness <= 1 && EndAttachment.IsValid() && SurfaceAttachment.IsValid() &&
                   (!HasGrowth ||
                    (Growth.IsValid() && OrderStep % (Growth.BranchCount + 1) == 0 &&
                     uint64_t(Growth.BranchCount) * Growth.BranchInterval < MaxPoints - 1 &&
                     (!EndAttachment.Enabled || Growth.Speed == 0 ||
                      Growth.State == EStrandGrowthState::Mature)));
        }
    };
}
