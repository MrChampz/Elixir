#pragma once

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

    // CoreV1 is byte-for-byte compatible with the current SGPUParticleState.
    // Future pool arenas and shader permutations will be selected from this key.
    enum class EParticleStateLayout : uint8_t
    {
        CoreV1 = 0
    };

    enum class EParticleOp : uint32_t
    {
        SetLiteral = 0,
        RandomRange,
        SampleDisk,
        SampleCone,
        SampleBox,
        AddWithDelta,
        Dampen,
        LerpOverLife,
        KillOutsideBounds,
        AddFromAttribute,
        SetPositionOnCircle,
        SetPositionCircularPath,
        SetPositionVortexRibbonPath,
        SetRibbonIdFromSpawnOrder,
        SampleCurve,
        SampleColorCurve,
        Add,
        Mul,
        Clamp,
        CopyFromAttribute,
        ApplyVortex,
    };
}