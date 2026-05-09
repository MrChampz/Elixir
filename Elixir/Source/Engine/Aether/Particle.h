#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace Elixir::Aether
{
    enum class EParticleAttribute : uint32_t
    {
        None = 0,
        Position,
        Velocity,
        Color,
        Size,
        Lifetime,
        Tangent,
        RibbonId
    };

    struct SParticle
    {
        glm::vec3 Position;
        glm::vec3 Velocity;
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        float Lifetime = 1.0f;
        float Age = 0.0f;
        float Size = 4.0f;
        uint32_t RibbonId = 0;
        bool Alive = false;
    };
}
