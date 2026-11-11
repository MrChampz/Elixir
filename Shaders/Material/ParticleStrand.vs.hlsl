#include "../Aether/ParticleData.hlsl"
#include "../Aether/Math.hlsl"

[[vk::binding(0, 0)]]
cbuffer cbFrame
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float Time;
};

[[vk::binding(1, 0)]]
StructuredBuffer<SGPUParticle> particles;

struct PushConstants
{
    float4x4 WorldTransform;
    uint ParticleOffset;
    uint ParticleCount;
    uint RadialSegments;
    uint Subdivisions;
    float WidthScale;
    uint OrderStep;
    uint MaterialIndex;
    uint MaxPoints;
};

[[vk::push_constant]]
PushConstants pc;

struct VSOutput
{
    float4 ClipPos              : SV_POSITION;
    float3 WorldPos             : POSITION0;
    float3 Normal               : NORMAL0;
    float2 TexCoord             : TEXCOORD0;
    float2 TexCoord1            : TEXCOORD1;
    float4 Color                : COLOR;
    nointerpolation float Valid : TEXCOORD2;
    float2 ParticleTime         : TEXCOORD3;
};

uint NextSlot(uint slot)
{
    uint step = pc.OrderStep % pc.ParticleCount;
    return slot >= pc.ParticleCount - step ? slot - (pc.ParticleCount - step) : slot + step;
}

uint PreviousSlot(uint slot)
{
    uint step = pc.OrderStep % pc.ParticleCount;
    return slot < step ? pc.ParticleCount - (step - slot) : slot - step;
}

bool Connected(SGPUParticle first, SGPUParticle second)
{
    return first.Alive >= 0.5 &&
        second.Alive >= 0.5 &&
        pc.OrderStep > 0 &&
        first.EmitterIndex == second.EmitterIndex &&
        first.GroupId == second.GroupId &&
        second.LinkOrder >= pc.OrderStep && 
        second.LinkOrder - pc.OrderStep == first.LinkOrder &&
        (pc.MaxPoints == 0 || (second.LinkOrder / pc.OrderStep) % pc.MaxPoints != 0);
}

void CatmullRom(
    float3 p0, 
    float3 p1,
    float3 p2,
    float3 p3,
    float u,
    out float3 position,
    out float3 tangent
)
{
    float u2 = u * u;
    float u3 = u2 * u;

    position = 0.5 * (2 * p1 + (-p0 + p2) * u +
        (2 * p0 - 5 * p1 + 4 * p2 - p3) * u2 + (-p0 + 3 * p1 - 3 * p2 + p3) * u3);
    tangent = 0.5 * (-p0 + p2 + (2 * p0 - 5 * p1 + 4 * p2 - p3) * (2 * u) +
        (-p0 + 3 * p1 - 3 * p2 + p3) * (3 * u2));
}

static const uint LONGITUDINAL[6] = { 0, 0, 1, 0, 1, 1 };
static const uint RADIAL_OFFSET[6] = { 0, 1, 1, 0, 1, 0 };

VSOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    VSOutput output = (VSOutput)0;
    output.ClipPos.w = 1;

    if (instanceId >= pc.ParticleCount || pc.OrderStep == 0)
        return output;

    uint nextSlot = NextSlot(instanceId);
    SGPUParticle first = particles[pc.ParticleOffset + instanceId];
    SGPUParticle second = particles[pc.ParticleOffset + nextSlot];

    if (!Connected(first, second))
        return output;

    SGPUParticle previous = particles[pc.ParticleOffset + PreviousSlot(instanceId)];
    SGPUParticle following = particles[pc.ParticleOffset + NextSlot(nextSlot)];

    float3 p0 = Connected(previous, first) ? previous.Position : first.Position;
    float3 p3 = Connected(second, following) ? following.Position : second.Position;

    uint verticesPerRing = pc.RadialSegments * 6u;
    uint section = vertexId / verticesPerRing;
    uint local = vertexId % verticesPerRing;
    uint radialQuad = local / 6u;
    uint corner = local % 6u;
    float u = float(section + LONGITUDINAL[corner]) / float(pc.Subdivisions);
    float angle = 6.28318530718 * float(radialQuad + RADIAL_OFFSET[corner]) /
        float(pc.RadialSegments);
    float3 position;
    float3 tangent;

    CatmullRom(p0, first.Position, second.Position, p3, u, position, tangent);
    tangent = SafeNormalize(tangent, float3(0, 1, 0));

    float3 reference = abs(tangent.z) < 0.9
        ? float3(0, 0, 1)
        : float3(0, 1, 0);

    float3 side = SafeNormalize(cross(tangent, reference), float3(1, 0, 0));
    float3 up = cross(tangent, side);

    float3 radial = cos(angle) * side + sin(angle) * up;
    float3 circumference = -sin(angle) * side + cos(angle) * up;
    float radius = lerp(first.Size * max(first.Transform.y, 0.0),
        second.Size * max(second.Transform.y, 0.0), u) * pc.WidthScale * 0.0016;

    float3 world = mul(pc.WorldTransform, float4(position + radial * radius, 1)).xyz;
    float3 worldTangent = mul((float3x3)pc.WorldTransform, tangent);
    float3 worldCircumference = mul((float3x3)pc.WorldTransform, circumference);

    float2 texCoord = float2(
        float(first.LinkOrder / pc.OrderStep) + u,
        float(radialQuad + RADIAL_OFFSET[corner]) / float(pc.RadialSegments)
    );

    output.ClipPos = mul(ViewProj, float4(world, 1));
    output.WorldPos = world;
    output.Normal = SafeNormalize(cross(worldCircumference, worldTangent), radial);
    output.TexCoord = texCoord;
    output.TexCoord1 = output.TexCoord;
    output.Color = lerp(first.Color, second.Color, u);
    output.ParticleTime = lerp(
        float2(first.Age, first.Lifetime),
        float2(second.Age, second.Lifetime),
        u
    );
    output.Valid = 1;

    return output;
}
