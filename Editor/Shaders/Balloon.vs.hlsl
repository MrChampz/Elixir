#include "../../Shaders/Aether/ParticleData.hlsl"
[[vk::binding(0, 0)]] cbuffer cbFrame
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float Time;
};
[[vk::binding(1, 0)]] StructuredBuffer<SGPUParticle> particles;
[[vk::binding(2, 0)]] StructuredBuffer<float4> strandPoints;
struct PushConstants
{
    uint ParticleOffset;
    uint PointOffset;
    uint PointCount;
    uint BranchCount;
    float Radius;
    float Height;
    float Tilt;
    float ColorVariation;
    float4 Color;
    uint Seed;
};
[[vk::push_constant]] PushConstants pc;
struct VSOutput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : POSITION0;
    float3 Normal : NORMAL0;
    float4 Color : COLOR;
    nointerpolation float Valid : TEXCOORD0;
};
float3 safeDirection(float3 value, float3 fallback)
{
    float squared = dot(value,value);
    return squared > .000001 ? value * rsqrt(squared) : fallback;
}
uint hashSeed(uint value)
{
    value ^= value >> 16u; value *= 0x7feb352du;
    value ^= value >> 15u; value *= 0x846ca68bu;
    return value ^ (value >> 16u);
}
float3 rainbow(float hue)
{
    float3 rgb = saturate(abs(frac(hue + float3(0,2.0/3.0,1.0/3.0)) * 6.0 - 3.0) - 1.0);
    return lerp(float3(.035,.035,.035), float3(1,1,1), rgb);
}
VSOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    uint base = pc.PointOffset + instanceId * pc.PointCount * (1u + pc.BranchCount);
    float grown = strandPoints[base * 3u + 2u].w;
    uint tip = min(pc.PointCount - 1u,max(1u,uint(ceil(grown))));
    float3 anchor = strandPoints[(base + tip) * 3u].xyz;
    float3 tangent = safeDirection(anchor - strandPoints[(base + tip - 1u) * 3u].xyz,float3(0,1,0));
    float3 up = safeDirection(lerp(float3(0,1,0),tangent,pc.Tilt),float3(0,1,0));
    float3 side = safeDirection(cross(up,abs(up.z) < .9 ? float3(0,0,1) : float3(1,0,0)),float3(1,0,0));
    float3 forward = cross(side,up);
    SGPUParticle root = particles[pc.ParticleOffset + instanceId];
    uint seed = hashSeed(root.LinkOrder ^ pc.Seed ^ (instanceId * 7919u));
    float variation = .9 + .2 * float(seed & 65535u) / 65535.0;
    float scale = max(root.Transform.y,0.0) * variation;
    float radius = pc.Radius * scale;
    float height = pc.Height * scale;
    float knotHeight = radius * .16;
    const uint along[6] = {0u,0u,1u,0u,1u,1u};
    const uint around[6] = {0u,1u,1u,0u,1u,0u};
    float3 local, localNormal;
    if (vertexId < 2304u)
    {
        uint quad = vertexId / 6u;
        uint corner = vertexId % 6u;
        float v = float(quad / 24u + along[corner]) / 16.0;
        float phi = 6.2831853 * float(quad % 24u + around[corner]) / 24.0;
        float angle = 3.14159265 * v;
        float ring = radius * sin(angle) * (.68 + .32 * v);
        local = float3(cos(phi) * ring,knotHeight + height * .5 * (1.0-cos(angle)),sin(phi) * ring);
        float dr = radius * (3.14159265 * cos(angle) * (.68 + .32 * v) + .32 * sin(angle));
        float dy = height * .5 * 3.14159265 * sin(angle);
        localNormal = safeDirection(float3(cos(phi) * dy,-dr,sin(phi) * dy),float3(0,1,0));
    }
    else
    {
        uint quad = (vertexId - 2304u) / 6u;
        uint corner = vertexId % 6u;
        float v = float(along[corner]);
        float phi = 6.2831853 * float(quad + around[corner]) / 8.0;
        float ring = radius * lerp(.055,.105,v);
        local = float3(cos(phi)*ring,v*knotHeight,sin(phi)*ring);
        localNormal = safeDirection(float3(cos(phi),-.3,sin(phi)),float3(0,1,0));
    }
    float3 worldPosition = anchor + side * local.x + up * local.y + forward * local.z;
    VSOutput output;
    output.WorldPos = worldPosition;
    output.ClipPos = mul(ViewProj,float4(worldPosition,1));
    output.Normal = side * localNormal.x + up * localNormal.y + forward * localNormal.z;
    float hue = float(hashSeed(seed + 17u) & 65535u) / 65535.0;
    output.Color = float4(pc.Color.rgb * lerp(float3(1,1,1),rainbow(hue),pc.ColorVariation),pc.Color.a);
    output.Valid = root.Alive >= .5 && grown > .001 ? 1.0 : 0.0;
    return output;
}
