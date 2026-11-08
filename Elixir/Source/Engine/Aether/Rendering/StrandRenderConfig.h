#pragma once

#include <Engine/Aether/Rendering/EmitterRenderConfig.h>

namespace Elixir::Aether::Rendering
{
    /** Configures the thickness and tessellation of rendered strand tubes. */
    struct ELIXIR_API SStrandRenderConfig final : SEmitterRenderConfig
    {
        /** Maximum particles per chain; zero leaves the chain length unrestricted. */
        uint32_t MaxPoints = 0;

        /** LinkOrder difference and ring-slot stride between connected particles. */
        uint32_t OrderStep = 1;

        /** Multiplier applied to particle size when computing the tube radius. */
        float WidthScale = 0.3f;

        /** Number of sides around each tube, from 3 to 12. */
        uint32_t RadialSegments = 8;

        /** Spline subdivisions per control segment, from 1 to 8. */
        uint32_t Subdivisions = 4;

        /** @brief Checks whether all visual values are valid. */
        bool IsValid() const override
        {
            return OrderStep > 0 && (MaxPoints == 0 || MaxPoints >= 2) &&
                std::isfinite(WidthScale) && WidthScale > 0.0f &&
                RadialSegments >= 3 && RadialSegments <= 12 &&
                Subdivisions >= 1 && Subdivisions <= 8;
        }

        /** @brief Returns the triangle-list vertex count for one particle connection. */
        uint32_t GetSegmentVertexCount() const
        {
            return Subdivisions * RadialSegments * 6;
        }
    };
}
