#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <cmath>

namespace Elixir::Aether::Core
{
    /** Selects the coordinate space of a fixed strand endpoint. */
    enum class EStrandEndSpace : uint32_t { Simulation, World };

    /** Pins the main chain's last point independently of surface attachment. */
    struct SStrandEndAttachment
    {
        /** Pins the endpoint when true; intermediate points still follow the solver. */
        bool Enabled = false;
        /** Fixed destination in the selected space, in simulation or world units. */
        glm::vec3 Position{};
        /** Simulation follows the emitter's simulation space; World stays fixed when the instance moves. */
        EStrandEndSpace Space = EStrandEndSpace::Simulation;

        /** Checks finite coordinates and a supported coordinate space. */
        bool IsValid() const
        {
            return std::isfinite(Position.x) && std::isfinite(Position.y) && std::isfinite(Position.z) &&
                uint32_t(Space) <= uint32_t(EStrandEndSpace::World);
        }
    };
}
