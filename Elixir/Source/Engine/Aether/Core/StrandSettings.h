#pragma once

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
        /** @brief Number of distance-constraint iterations per internal step, from 1 to 16. */
        uint32_t SolverIterations = 8;
        /** @brief Number of sides around each tube, from 3 to 12. */
        uint32_t RadialSegments = 8;
        /** @brief Spline subdivisions per control segment, from 1 to 8. */
        uint32_t Subdivisions = 4;

        /** @brief Checks numeric ranges before settings enter compiled or GPU data. */
        bool IsValid() const
        {
            return PointCount >= 2 && PointCount <= 96 &&
                std::isfinite(SegmentLength) && SegmentLength > 0.0f &&
                std::isfinite(Stiffness) && Stiffness >= 0.0f && Stiffness <= 1.0f &&
                std::isfinite(Damping) && Damping >= 0.0f && Damping <= 1.0f &&
                std::isfinite(WidthScale) && WidthScale > 0.0f &&
                SolverIterations >= 1 && SolverIterations <= 16 &&
                RadialSegments >= 3 && RadialSegments <= 12 && Subdivisions >= 1 && Subdivisions <= 8;
        }

        /** @brief Returns the triangle-list vertex count for one tube. */
        uint32_t GetVertexCount() const
        {
            return (PointCount - 1) * Subdivisions * RadialSegments * 6;
        }
    };
}
