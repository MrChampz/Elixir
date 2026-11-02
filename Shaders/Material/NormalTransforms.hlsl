#ifndef ELIXIR_MATERIAL_NORMAL_TRANSFORMS_HLSL
#define ELIXIR_MATERIAL_NORMAL_TRANSFORMS_HLSL

// Build a world-space basis from a normal and a tangent with handedness in w.
float3x3 BuildTangentBasis(float3 normal, float4 tangent)
{
    const float3 direction = normalize(tangent.xyz - normal * dot(normal, tangent.xyz));
    const float3 bitangent = cross(normal, direction) * tangent.w;
    return float3x3(direction, bitangent, normal);
}

// Transform a tangent-space normal through a supplied world-space basis.
float3 TangentNormalToWorld(float3 tangentNormal, float3x3 tangentBasis)
{
    return normalize(mul(normalize(tangentNormal), tangentBasis));
}

// Reconstruct the basis from screen-space derivatives in a pixel shader.
// Keep the geometric normal when the UV mapping cannot provide a usable tangent.
float3 TangentNormalToWorld(
    float3 tangentNormal,
    float3 geometricNormal,
    float3 worldPosition,
    float2 texCoord
)
{
    const float3 positionX = ddx(worldPosition);
    const float3 positionY = ddy(worldPosition);
    const float2 texCoordX = ddx(texCoord);
    const float2 texCoordY = ddy(texCoord);
    const float determinant = texCoordX.x * texCoordY.y - texCoordX.y * texCoordY.x;

    if (abs(determinant) > 1e-6f)
    {
        const float3 uvTangent =
            (positionX * texCoordY.y - positionY * texCoordX.y) / determinant;
        const float3 uvBitangent =
            (positionY * texCoordX.x - positionX * texCoordY.x) / determinant;
        const float tangentLengthSquared = dot(uvTangent, uvTangent);

        if (tangentLengthSquared > 1e-6f)
        {
            const float handedness = dot(cross(uvTangent, uvBitangent), geometricNormal) < 0.0f
                ? -1.0f
                : 1.0f;
            const float3x3 tangentBasis = BuildTangentBasis(
                geometricNormal,
                float4(uvTangent, handedness)
            );
            return TangentNormalToWorld(tangentNormal, tangentBasis);
        }
    }

    return geometricNormal;
}

#endif
