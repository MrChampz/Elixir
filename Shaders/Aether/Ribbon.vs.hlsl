struct ParticleState
{
    float4 PositionSize;    // xyz = position, w = size
    float4 VelocityAge;     // xyz = velocity, w = age
    float4 Transform;       // x = rotation, y = scale
    float4 TangentRibbonId; // xyz = tangent, w = ribbon id
    float4 Color;
    float4 Metadata;        // x = emitter index, y = ribbon link order, z = lifetime, w = alive
};

[[vk::binding(1, 0)]]
StructuredBuffer<ParticleState> particles;

struct Emitter
{
    float4 MetaA; // x = offset in particle buffer, y = max particles, z = module offset(spawn), w = module count(spawn)
    float4 MetaB; // x = module offset(update), y = module count(update), z = buffer cursor, w = spawn count
    float4 MetaC; // x = render mode, y = spawn rate seconds, z = gravity scale, w = next buffer cursor
    float4 MetaD; // x = emission index
};

[[vk::binding(2, 0)]]
StructuredBuffer<Emitter> emitters;

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float _Padding;
};

struct PushConstants
{
    uint EmitterIndex;
};

[[vk::push_constant]]
PushConstants pc;

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
    nointerpolation float Valid : TEXCOORD1;
};

#include "Common.hlsl"

static const float RIBBON_WORLD_SIZE_SCALE = 0.01;

VSOutput EmptyVertex()
{
    VSOutput output;
    output.ClipPos = float4(0.0, 0.0, 0.0, 1.0);
    output.Color = float4(0.0, 0.0, 0.0, 0.0);
    output.UV = float2(0.5, 0.0);
    output.Valid = 0.0;
    return output;
}

VSOutput main(uint vertexId : SV_VertexID)
{
    Emitter emitter = emitters[pc.EmitterIndex];
    uint particleCount = (uint)emitter.MetaA.y;
    if (particleCount == 0u)
        return EmptyVertex();

    uint segmentIndex = vertexId / 6u;
    uint vertexInSegment = vertexId % 6u;
    if (segmentIndex >= particleCount)
        return EmptyVertex();

    ParticleState startParticle;
    ParticleState endParticle;
    uint endLocalIndex;

    if (!TryBuildSegment(segmentIndex, emitter, startParticle, endParticle, endLocalIndex))
        return EmptyVertex();

    float3 p0 = startParticle.PositionSize.xyz;
    float3 p1 = endParticle.PositionSize.xyz;
    float scale0 = max(startParticle.Transform.y, 0.0);
    float scale1 = max(endParticle.Transform.y, 0.0);
    float width0 = max(startParticle.PositionSize.w * scale0 * RIBBON_WORLD_SIZE_SCALE, 0.0001);
    float width1 = max(endParticle.PositionSize.w * scale1 * RIBBON_WORLD_SIZE_SCALE, 0.0001);
    float3 segmentSide = BuildSegmentSide(p0, p1, startParticle, endParticle);
    float3 startSide = BuildParticleSide(startParticle, segmentSide);
    float3 endSide = BuildParticleSide(endParticle, segmentSide);

    bool useEndParticle = vertexInSegment == 1u || vertexInSegment == 2u || vertexInSegment == 4u;
    bool usePositiveSide = vertexInSegment == 2u || vertexInSegment == 4u || vertexInSegment == 5u;

    float3 center = useEndParticle ? p1 : p0;
    float width = useEndParticle ? width1 : width0;
    float3 side = useEndParticle ? endSide : startSide;
    float sideSign = usePositiveSide ? 1.0 : -1.0;

    VSOutput output;
    output.ClipPos = mul(ViewProj, float4(center + side * (width * 0.5 * sideSign), 1.0));
    output.Color = useEndParticle ? endParticle.Color : startParticle.Color;
    output.UV = float2(usePositiveSide ? 1.0 : 0.0, useEndParticle ? 1.0 : 0.0);
    output.Valid = 1.0;

    return output;
}