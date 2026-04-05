#pragma once

#include <glm/glm.hpp>

namespace Elixir::Aether
{
    struct SParticle
    {
        glm::vec2 Position;
        glm::vec2 Velocity;
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        float Lifetime = 1.0f;
        float Age = 0.0f;
        float Size = 4.0f;
        bool Alive = false;
    };
}