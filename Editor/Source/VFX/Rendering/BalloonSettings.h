#pragma once
#include "../VFXDocument.h"
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace EditorVFX
{
    /** Configures the Editor's balloon geometry at the moving tip of each main strand. */
    struct BalloonSettings
    {
        /** Allows hiding balloons while retaining the cords and settings. */
        bool Enabled = true;
        /** Maximum body radius in simulation-space units. Must be positive. */
        float Radius = .11f;
        /** Body height in simulation-space units. Must be positive. */
        float Height = .28f;
        /** Blend from world up to the final cord direction, from zero to one. */
        float Tilt = .4f;
        /** Blend from the tint to seeded rainbow colors, from zero to one. */
        float ColorVariation = 1;
        /** Linear RGBA tint; RGB is nonnegative, alpha ranges from zero to one. */
        std::array<float,4> Color{1,1,1,1};
        /** Reproducible seed for color and size variation. */
        uint32_t Seed = 42;
        /** Checks finite dimensions and supported authoring ranges. */
        bool IsValid() const;
    };
    /** Parses application renderer settings per emitter; failure preserves the previous output. */
    bool ParseBalloonRenderers(const Value& effect, std::vector<std::optional<BalloonSettings>>& output,
        std::string& error);
}
