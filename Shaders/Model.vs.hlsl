// glTF mesh vertex shader. Transforms the vertex by the per-primitive model
// matrix (push constant) and the camera's view-projection.

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float _Padding;
};

struct ModelPushConstants
{
    float4x4 Model;
    float4 BaseColorFactor;
    uint TextureIndex;
};
[[vk::push_constant]]
ModelPushConstants pc;

struct VSInput
{
    float3 Position : POSITION0;
    float3 Normal   : NORMAL0;
    float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 ClipPos  : SV_POSITION;
    float3 Normal   : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float3 WorldPos : POSITION0;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 worldPos = mul(pc.Model, float4(input.Position, 1.0f));
    output.ClipPos = mul(ViewProj, worldPos);
    output.WorldPos = worldPos.xyz;
    // Assumes uniform scale; good enough until a dedicated normal matrix.
    output.Normal = normalize(mul((float3x3)pc.Model, input.Normal));
    output.TexCoord = input.TexCoord;

    return output;
}
