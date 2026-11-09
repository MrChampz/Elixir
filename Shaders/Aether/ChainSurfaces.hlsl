struct SConstraintSurfaceData
{
    float4 CenterType;
    float4 NormalRadius;
    float4 ExtentsRounding;
};

[[vk::binding(1, 0)]] cbuffer cbConstraint
{
    float4x4 ChainWorld;
    float4x4 ChainSimulation;
    uint4 ChainTopology;
    uint4 ChainCounts;
    float4 ChainGrowth;
    float4 ChainEndPosition;
    float4 ChainSurface;
    float4 ChainClimbDelay;
    SConstraintSurfaceData ChainSurfaces[8];
};

// Signed distance and an outward normal in world space.
float SurfaceDistance(float3 position, SConstraintSurfaceData shape, out float3 normal)
{
    float3 local = position - shape.CenterType.xyz;
    uint type = uint(shape.CenterType.w);
    if (type == 0)
    {
        normal = shape.NormalRadius.xyz;
        return dot(local, normal);
    }
    if (type == 1)
    {
        normal = SafeNormalize(local, float3(0, 0, 1));
        return length(local) - shape.NormalRadius.w;
    }
    float rounding = shape.ExtentsRounding.w;
    float3 q = abs(local) - (shape.ExtentsRounding.xyz - rounding);
    float3 outside = max(q, 0);
    float3 signValue = float3(local.x >= 0 ? 1 : -1,
        local.y >= 0 ? 1 : -1, local.z >= 0 ? 1 : -1);
    if (dot(outside, outside) > 0.0000001)
        normal = normalize(outside) * signValue;
    else
    {
        normal = q.x >= q.y && q.x >= q.z ? float3(signValue.x, 0, 0) :
            (q.y >= q.z ? float3(0, signValue.y, 0) : float3(0, 0, signValue.z));
    }
    return length(outside) + min(max(q.x, max(q.y, q.z)), 0) - rounding;
}

bool FindSurface(float3 position, out float distance, out float3 normal)
{
    float3 world = mul(ChainWorld, float4(position, 1)).xyz;
    distance = 1e20;
    normal = float3(0, 0, 1);
    for (uint index = 0; index < ChainCounts.y; ++index)
    {
        float3 candidateNormal;
        float candidate = SurfaceDistance(world, ChainSurfaces[index], candidateNormal);
        if (abs(candidate - ChainSurface.y) < abs(distance - ChainSurface.y))
        {
            distance = candidate;
            normal = candidateNormal;
        }
    }
    return abs(distance - ChainSurface.y) <= ChainSurface.x;
}

float3 AttachSurface(float3 position)
{
    float distance;
    float3 normal;
    if (!FindSurface(position, distance, normal)) return position;
    float3 world = mul(ChainWorld, float4(position, 1)).xyz;
    world -= normal * (distance - ChainSurface.y);
    return mul(ChainSimulation, float4(world, 1)).xyz;
}

uint ChainHash(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352d;
    value ^= value >> 15;
    value *= 0x846ca68b;
    return value ^ (value >> 16);
}

float ChainRandom(uint value)
{
    return float(ChainHash(value) & 65535) / 65535.0;
}
