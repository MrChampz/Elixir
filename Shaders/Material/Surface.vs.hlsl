// Template for EMaterialUsage::Surface.

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
    float       ScreenWidth;
    float       ScreenHeight;
    float4      LightDirection;
    float4      LightColor;
};

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

VSOutput main(VSInput input)
{
    VSOutput output;

    float3 worldPos = mul(pc.WorldTransform, float4(input.Position, 1.0f)).xyz;
    float3 worldNormal = normalize(mul((float3x3)pc.WorldTransform, input.Normal));
    float3 worldTangent = normalize(mul((float3x3)pc.WorldTransform, input.Tangent.xyz));

    output.ClipPos = mul(ViewProj, float4(worldPos, 1.0));
    output.WorldPos = worldPos;
    output.Normal = worldNormal;
    output.Tangent = float4(worldTangent, input.Tangent.w);
    output.TexCoord = input.TexCoord;
    output.TexCoord1 = input.TexCoord1;

    return output;
}