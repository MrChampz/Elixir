#ifndef AETHER_STRAND_SURFACES_HLSL
#define AETHER_STRAND_SURFACES_HLSL

struct SStrandSurfaces {
    float4 Controls; // count, search radius, offset, climb acceleration
    float4 Direction; // preferred xyz, attachment break stretch ratio
    float4 Shapes[24]; // center/type, half-extents/radius, normal/rounding
};
[[vk::binding(8, 0)]] StructuredBuffer<SStrandSurfaces> strandSurfaces;

void QuerySurface(SStrandSurfaces config, uint index, float3 position,
    out float distance, out float3 normal, out float3 closest)
{
    float4 centerType = config.Shapes[index * 3u];
    float4 size = config.Shapes[index * 3u + 1u];
    float4 plane = config.Shapes[index * 3u + 2u];
    float3 local = position - centerType.xyz;
    uint type = uint(centerType.w);
    if (type == 0u) {
        normal = plane.xyz;
        distance = dot(local, normal);
    } else if (type == 1u) {
        float radius = length(local);
        normal = safeNormalize3(local, float3(0, 0, 1));
        distance = radius - size.w;
    } else {
        float3 inner = size.xyz - plane.w;
        float3 q = abs(local) - inner;
        float3 outside = max(q, 0.0);
        float3 signs = float3(local.x < 0 ? -1 : 1, local.y < 0 ? -1 : 1, local.z < 0 ? -1 : 1);
        float outsideLength = length(outside);
        if (outsideLength > 0.000001) normal = outside / outsideLength * signs;
        else if (q.x >= q.y && q.x >= q.z) normal = float3(signs.x, 0, 0);
        else if (q.y >= q.z) normal = float3(0, signs.y, 0);
        else normal = float3(0, 0, signs.z);
        distance = outsideLength + min(max(q.x, max(q.y, q.z)), 0.0) - plane.w;
    }
    closest = position - normal * distance;
}

bool FindSurface(SStrandSurfaces config, float3 position, out uint index,
    out float3 closest, out float3 normal)
{
    index = 0u;
    closest = position;
    normal = float3(0, 0, 1);
    float best = config.Controls.y;
    bool found = false;
    bool insideSolid = false;
    for (uint surface = 0; surface < uint(config.Controls.x); ++surface) {
        float distance;
        float3 candidateNormal, candidate;
        QuerySurface(config, surface, position, distance, candidateNormal, candidate);
        bool penetrating = config.Shapes[surface * 3u].w > 0.5 && distance < 0;
        if ((penetrating && !insideSolid) || (penetrating == insideSolid && abs(distance) < best)) {
            found = true;
            insideSolid = penetrating;
            best = abs(distance);
            index = surface;
            closest = candidate;
            normal = candidateNormal;
        }
    }
    return found;
}

float3 SurfaceDirection(SStrandSurfaces config, float3 normal, float3 fallback)
{
    float3 preferred = config.Direction.xyz - normal * dot(config.Direction.xyz, normal);
    float3 heading = fallback - normal * dot(fallback, normal);
    float3 axis = abs(normal.z) < 0.9 ? float3(0, 0, 1) : float3(1, 0, 0);
    heading = safeNormalize3(heading, safeNormalize3(cross(normal, axis), float3(0, 1, 0)));
    if (dot(preferred, preferred) < 0.000001) return heading;
    preferred = normalize(preferred);
    // Preserve forward motion instead of reversing at a convex corner or pole.
    if (dot(preferred, heading) < -0.2) return heading;
    return safeNormalize3(preferred + heading * 0.65, heading);
}
#endif
