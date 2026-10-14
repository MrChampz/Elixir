#pragma once

#include "../VFXDocument.h"
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace EditorVFX
{
    /** Selects a leaf silhouette implemented by the Editor shaders. */
    enum class ELeafShape : uint32_t { Ivy, Lanceolate };

    /** Configures the Editor's optional leaf renderer independently of strand simulation. */
    struct LeafSettings
    {
        /** Allows disabling leaves without removing their settings. */
        bool Enabled = true;
        /** Control segments between leaves, from one through 96. */
        uint32_t Interval = 3;
        /** Blade length in simulation-space units. Must be positive. */
        float Length = .12f;
        /** Blade width in simulation-space units. Must be positive. */
        float Width = .1f;
        /** Development-front distance needed to fully open a leaf, in segments. */
        float MaturationSegments = 2;
        /** Linear RGBA color; RGB is nonnegative and alpha ranges from zero to one. */
        std::array<float, 4> Color{.12f,.45f,.08f,1};
        /** Procedural silhouette; this has no effect on strand physics. */
        ELeafShape Shape = ELeafShape::Ivy;

        /** Checks finite values and supported authoring ranges. */
        bool IsValid() const;
        /** Counts leaves strictly between the root and the tip capacity. */
        uint32_t GetLeafCount(uint32_t pointCount) const;
    };

    /** Parses Editor renderer settings per emitter. Failure leaves the output unchanged. */
    bool ParseLeafRenderers(const Value& effect, std::vector<std::optional<LeafSettings>>& output,
        std::string& error);
}
