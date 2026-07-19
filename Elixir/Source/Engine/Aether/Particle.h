#pragma once

#include <glm/glm.hpp>

namespace Elixir::Aether
{
    enum class EParticleAttribute : uint32_t
    {
        None = 0,
        Position,
        Rotation,
        Scale,
        Velocity,
        Color,
        Size,
        Lifetime,
        Tangent,
        RibbonId,
        Temp0,
        Temp1,
        Temp2,
        Temp3,
    };

    enum class EParticleRenderMode : uint8_t
    {
        Sprite = 0,
        Ribbon = 1,
        Mesh   = 2
    };

    // CoreV1 is byte-for-byte compatible with the current SGPUParticleState.
    // Future pool arenas and shader permutations will be selected from this key.
    enum class EParticleStateLayout : uint8_t
    {
        CoreV1 = 0
    };

    struct SParticle
    {
        glm::vec3 Position;
        float Rotation = 0.0f;
        float Scale = 1.0f;
        glm::vec3 Velocity;
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        float Lifetime = 1.0f;
        float Age = 0.0f;
        float Size = 4.0f;
        bool Alive = false;
        uint32_t RibbonId = 0;
    };
}