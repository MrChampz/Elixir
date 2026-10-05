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

    /** @brief Identifies the GPU operation executed by a particle instruction. */
    enum class EParticleOp : uint32_t
    {
        SetLiteral = 0,
        RandomRange = 1,
        SampleDisk = 2,
        SampleCone = 3,
        SampleBox = 4,
        AddWithDelta = 5,
        Dampen = 6,
        LerpOverLife = 7,
        KillOutsideBounds = 8,
        AddFromAttribute = 9,
        SetPositionOnCircle = 10,
        SetPositionCircularPath = 11,
        SetPositionVortexRibbonPath = 12,
        SetRibbonIdFromSpawnOrder = 13,
        SampleCurve = 14,
        SampleColorCurve = 15,
        Add = 16,
        Mul = 17,
        Clamp = 18,
        CopyFromAttribute = 19,
        ApplyVortex = 20,
    };
}
