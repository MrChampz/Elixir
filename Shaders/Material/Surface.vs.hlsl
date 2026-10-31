// Template for EMaterialUsage::Surface.

#include "../Material/NormalSampling.hlsl"

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4    View;
    float4x4    Proj;
    float4x4    ViewProj;
    float3      CameraPos;
    float       Time;
    float       EnvIntensity;
    float       EnvMaxLod;
    uint        SceneColorIndex;
    uint        DebugView;
    float       ScreenWidth;
    float       ScreenHeight;
    float4      LightDirection;
    float4      LightColor;
};

[[vk::binding(1, 1)]]
Texture2D textures[];

[[vk::binding(1, 0)]]
SamplerState texSampler;

struct CompiledMaterial
{
    float4 Values[32];
    uint TextureIndices[32];
    uint BlendMode;
    float AlphaCutoff;
};

[[vk::binding(2, 0)]]
StructuredBuffer<CompiledMaterial> materials;

struct MaterialPushConstants
{
    float4x4    WorldTransform;
    uint        MaterialIndex;
};

[[vk::push_constant]]
MaterialPushConstants pc;

struct VSInput
{
    float3 Position     : POSITION0;
    float3 Normal       : NORMAL0;
    float4 Tangent      : TANGENT0;
    float2 TexCoord     : TEXCOORD0;
    float2 TexCoord1    : TEXCOORD1;
};

struct VSOutput
{
    float4 ClipPos      : SV_Position;
    float3 Normal       : NORMAL0;
    float4 Tangent      : TANGENT0;
    float2 TexCoord     : TEXCOORD0;
    float2 TexCoord1    : TEXCOORD1;
    float3 WorldPos     : POSITION0;
};

[[vk::binding(8, 0)]]
SamplerState linearClamp;

[[vk::binding(9, 0)]]
SamplerState pointWrap;

[[vk::binding(10, 0)]]
SamplerState pointClamp;

// Evaluate the same graph at nearby positions to preserve smooth mesh normals.
float3 EvaluateWorldPositionOffset(float3 worldPos, VSInput input)
{
    CompiledMaterial mat = materials[pc.MaterialIndex];
    float3 worldPositionOffset = float3(0.0, 0.0, 0.0);

    // __GRAPH_BODY__

    return worldPositionOffset;
}

VSOutput main(VSInput input)
{
    VSOutput output;

    float3 worldPos = mul(pc.WorldTransform, float4(input.Position, 1.0f)).xyz;
    float3 worldNormal = normalize(mul((float3x3)pc.WorldTransform, input.Normal));
    float3 worldTangent = normalize(mul((float3x3)pc.WorldTransform, input.Tangent.xyz));

    const float3 displacedPosition = worldPos + EvaluateWorldPositionOffset(worldPos, input);

#if GRAPH_HAS_WPO
    if (dot(input.Tangent.xyz, input.Tangent.xyz) > 1e-8)
    {
        const float3 tangent = normalize(worldTangent - worldNormal *
            dot(worldNormal, worldTangent));
        const float3 bitangent = cross(worldNormal, tangent) * input.Tangent.w;

        const float epsilon = 0.01;

        const float3 tangentPosition = worldPos + tangent * epsilon;
        const float3 bitangentPosition = worldPos + bitangent * epsilon;

        const float3 displacedTangent = tangentPosition +
            EvaluateWorldPositionOffset(tangentPosition, input) - displacedPosition;
        const float3 displacedBitangent = bitangentPosition +
            EvaluateWorldPositionOffset(bitangentPosition, input) - displacedPosition;

        const float3 normal = cross(displacedTangent, displacedBitangent);

        if (dot(normal, normal) > 1e-12)
        {
            const float3 candidate = normalize(normal);
            worldNormal = dot(candidate, worldNormal) < 0.0 ? -candidate : candidate;
        }
    }
#endif

    worldPos = displacedPosition;

    output.ClipPos = mul(ViewProj, float4(worldPos, 1.0));
    output.WorldPos = worldPos;
    output.Normal = worldNormal;
    output.Tangent = float4(worldTangent, input.Tangent.w);
    output.TexCoord = input.TexCoord;
    output.TexCoord1 = input.TexCoord1;

    return output;
}