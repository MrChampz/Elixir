#pragma once

#include <cstdint>

namespace Elixir::Aether::Core
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

    enum class EParticleSimulationSpace : uint8_t
    {
        World = 0,
        Local = 1,
    };

    enum class EParticleOp : uint32_t
    {
#define AETHER_OPERATION(Name, Opcode, ...) Name = Opcode,
#define AETHER_ARGUMENT(Op, Name, Index)
#include "ParticleOperations.def"
#undef AETHER_ARGUMENT
#undef AETHER_OPERATION
    };
}