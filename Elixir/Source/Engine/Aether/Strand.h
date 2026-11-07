#pragma once

#include <cmath>
#include <cstdint>

namespace Elixir::Aether::Simulation
{
    /** Configures the point chain and constraint solver for each strand. */
    struct SStrandSimulationConfig
    {
        /** Number of persistent control points, including the root; from 2 to 96. */
        uint32_t PointCount = 24;

        /** Rest length between consecutive points, in simulation-space units. */
        float SegmentLength = 0.04f;

        /** Fraction of distance error corrected per solver iteration, from 0 to 1. */
        float Stiffness = 0.85f;

        /** Fraction of velocity retained over a 1/60-second step, from 0 to 1. */
        float Damping = 0.96f;

        /** Number of distance-constraint iterations per internal step, from 1 to 16. */
        uint32_t SolverIterations = 8;

        /** @brief Checks whether all solver values are valid. */
        bool IsValid() const
        {
            return PointCount >= 2 && PointCount <= 96 &&
                std::isfinite(SegmentLength) && SegmentLength > 0.0f &&
                std::isfinite(Stiffness) && Stiffness >= 0.0f && Stiffness <= 1.0f &&
                std::isfinite(Damping) && Damping >= 0.0f && Damping <= 1.0f &&
                SolverIterations >= 1 && SolverIterations <= 16;
        }
    };
}

namespace Elixir::Aether::Rendering
{
    /** Configures the thickness and tessellation of rendered strand tubes. */
    struct SStrandRenderConfig
    {
        /** Multiplier applied to particle size when computing the tube radius. */
        float WidthScale = 0.3f;

        /** Number of sides around each tube, from 3 to 12. */
        uint32_t RadialSegments = 8;

        /** Spline subdivisions per control segment, from 1 to 8. */
        uint32_t Subdivisions = 4;

        /** @brief Checks whether all visual values are valid. */
        bool IsValid() const
        {
            return std::isfinite(WidthScale) && WidthScale > 0.0f &&
                RadialSegments >= 3 && RadialSegments <= 12 &&
                Subdivisions >= 1 && Subdivisions <= 8;
        }

        /**
         * @brief Returns the triangle-list vertex count for one strand tube.
         * @param pointCount Number of simulated control points, including the root.
         * @pre This configuration is valid and pointCount is between 2 and 96.
         */
        uint32_t GetVertexCount(const uint32_t pointCount) const
        {
            return (pointCount - 1) * Subdivisions * RadialSegments * 6;
        }
    };
}

namespace Elixir::Aether
{
    /** Stores strand-specific compiled data for one emitter in a compiled system. */
    struct SCompiledStrand
    {
        /** Index of the source emitter in SCompiledSystem::Emitters. */
        uint32_t EmitterIndex = 0;

        /** Point-chain and solver configuration copied from the authored emitter. */
        Simulation::SStrandSimulationConfig SimulationConfig;

        /** Tube thickness and tessellation copied from the authored emitter. */
        Rendering::SStrandRenderConfig RenderConfig;

        /** First point relative to the containing system's strand point range. */
        uint32_t LocalPointOffset = 0;

        /** First point-update instruction in the containing system's operation table. */
        uint32_t UpdateOperationOffset = 0;

        /** Number of instructions executed for each free strand point. */
        uint32_t UpdateOperationCount = 0;
    };
}
