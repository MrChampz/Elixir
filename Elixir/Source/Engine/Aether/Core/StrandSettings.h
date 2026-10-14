#pragma once

#include "StrandSurfaceSettings.h"
#include "StrandGrowthSettings.h"
#include "StrandEndAttachment.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <cmath>

namespace Elixir::Aether::Core
{
    /** @brief Configures a GPU strand rooted at each particle. Lengths use simulation-space units. */
    struct SStrandSettings
    {
        /** @brief Number of persistent control points, including the root; valid range is 2 to 96. */
        uint32_t PointCount = 24;
        /** @brief Rest length of each segment in simulation-space units. */
        float SegmentLength = 0.04f;
        /** @brief Fraction of distance error corrected per solver iteration, from 0 to 1. */
        float Stiffness = 0.85f;
        /** @brief Velocity retained over a 1/60-second step, from 0 to 1. */
        float Damping = 0.96f;
        /** @brief Scale applied to the particle size when computing tube radius. */
        float WidthScale = 0.3f;
        /** Tube radius multiplier at the root; zero closes the root. Must be nonnegative. */
        float RootWidthScale = 1.0f;
        /** Tube radius multiplier at the grown tip; zero closes the tip. Must be nonnegative. */
        float TipWidthScale = 0.0f;
        /** Positive exponent of normalized length; one gives a linear radius transition. */
        float WidthExponent = 1.0f;
        /** Additional tube radius multiplier for side chains. Must be nonnegative. */
        float BranchWidthScale = 0.65f;
        /** @brief Number of distance-constraint iterations per internal step, from 1 to 16. */
        uint32_t SolverIterations = 8;
        /** @brief Number of sides around each tube, from 3 to 12. */
        uint32_t RadialSegments = 8;
        /** @brief Spline subdivisions per control segment, from 1 to 8. */
        uint32_t Subdivisions = 4;

        /** @brief Tip growth speed in simulation units per second; zero starts fully grown. */
        float GrowthSpeed = 0;
        /** @brief Maximum grown length; zero uses all configured segments. */
        float GrowthMaxLength = 0;
        /** @brief Number of side chains per root, from zero to four. */
        uint32_t BranchCount = 0;
        /** @brief Control segments between branch attachment points; must keep attachments before the tip. */
        uint32_t BranchInterval = 8;
        /** @brief Side-chain length and growth-speed multiplier, greater than zero and at most one. */
        float BranchLengthScale = 0.5f;

        /** Optional second pin on fully grown main chains; branches keep free tips. */
        SStrandEndAttachment EndAttachment;

        /** @brief Static world-space surfaces followed by growing tips. */
        SStrandSurfaceSettings SurfaceAttachment;

        /** Development-stage authoring data compiled from StrandGrowth when present. */
        SStrandGrowthSettings Growth;
        /** Selects the independent development clock; false preserves legacy asset timing. */
        bool HasGrowthModule = false;

        /** @brief Checks numeric ranges before settings enter compiled or GPU data. */
        bool IsValid() const
        {
            return PointCount >= 2 && PointCount <= 96 &&
                std::isfinite(SegmentLength) && SegmentLength > 0.0f &&
                std::isfinite(Stiffness) && Stiffness >= 0.0f && Stiffness <= 1.0f &&
                std::isfinite(Damping) && Damping >= 0.0f && Damping <= 1.0f &&
                std::isfinite(WidthScale) && WidthScale > 0.0f &&
                std::isfinite(RootWidthScale) && RootWidthScale >= 0 &&
                std::isfinite(TipWidthScale) && TipWidthScale >= 0 &&
                std::isfinite(WidthExponent) && WidthExponent > 0 &&
                std::isfinite(BranchWidthScale) && BranchWidthScale >= 0 &&
                SolverIterations >= 1 && SolverIterations <= 16 &&
                RadialSegments >= 3 && RadialSegments <= 12 && Subdivisions >= 1 && Subdivisions <= 8 &&
                std::isfinite(GrowthSpeed) && GrowthSpeed >= 0 &&
                std::isfinite(GrowthMaxLength) && GrowthMaxLength >= 0 &&
                BranchCount <= 4 && BranchInterval >= 1 &&
                (!BranchCount || uint64_t(BranchCount) * BranchInterval < PointCount - 1) &&
                std::isfinite(BranchLengthScale) && BranchLengthScale > 0 && BranchLengthScale <= 1 && SurfaceAttachment.IsValid() && (!HasGrowthModule || Growth.IsValid()) &&
                EndAttachment.IsValid() && (!EndAttachment.Enabled ||
                    (GrowthSpeed == 0 && GrowthMaxLength == 0 && (!HasGrowthModule || (Growth.Speed == 0 && Growth.MaxLength == 0))));
        }

        /** @brief Returns reserved control points for the main chain and its branches. */
        uint32_t GetStoragePointCount() const { return PointCount * (1 + BranchCount); }

        /** @brief Returns the tube triangle-list vertex count. */
        uint32_t GetVertexCount() const
        {
            return (PointCount - 1) * Subdivisions * RadialSegments * 6;
        }
    };
}
