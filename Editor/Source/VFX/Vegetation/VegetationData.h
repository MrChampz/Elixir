#pragma once

#include "VegetationSettings.h"
#include <Engine/Core/Core.h>
#include <glm/glm.hpp>
#include <optional>

namespace EditorVFX
{
    using namespace Elixir;
    /** Packs one analytic world surface for the constraint shader. */
    struct SConstraintSurfaceData
    {
        /** World center in xyz and analytic shape type in w. */
        glm::vec4 CenterType{};
        /** Unit plane normal in xyz and sphere radius in w. */
        glm::vec4 NormalRadius{};
        /** Box half extents in xyz and corner radius in w. */
        glm::vec4 ExtentsRounding{};
    };

    /** Uniform data for one dispatch; every field starts on a 16-byte boundary. */
    struct SConstraintFrameData
    {
        /** Transform from simulation coordinates to world coordinates. */
        glm::mat4 World{1};
        /** Transform from world coordinates to simulation coordinates. */
        glm::mat4 Simulation{1};
        /** Growth enabled, branch count, branch interval, and initial development state. */
        glm::uvec4 Topology{};
        /** Seed, active surface count, endpoint enabled, and endpoint coordinate space. */
        glm::uvec4 Counts{};
        /** Growth speed, maximum length, branch length scale, and node variation. */
        glm::vec4 Growth{};
        /** Endpoint position in the space selected by Counts.w; w is unused. */
        glm::vec4 EndPosition{};
        /** Search radius, surface offset, climb acceleration, and release stretch ratio. */
        glm::vec4 Surface{};
        /** World climb direction and maximum branch delay in seconds. */
        glm::vec4 ClimbDelay{0, 1, 0, 0};
        /** Static world surfaces; Counts.y selects the active prefix. */
        std::array<SConstraintSurfaceData, SStrandSurfaceSettings::Capacity> Surfaces{};
    };

    /**
     * @brief Packs validated settings and coordinate transforms for a constraint dispatch.
     * @param config Constraint settings from the compiled system.
     * @param world Transform from simulation coordinates to world coordinates.
     * @return Uniform data, or no data for invalid settings or a singular transform.
     */
    std::optional<SConstraintFrameData> BuildConstraintFrameData(
        const VegetationSettings& config,
        const glm::mat4& world
    );
}
