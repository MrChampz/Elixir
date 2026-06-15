[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float _Padding;
};

struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float4 Color : COLOR0;
    float3 Normal : NORMAL0;
    float3 WorldPos : POSITION0;
};

static const float3 LIGHT_DIRECTION = float3(-0.45, 0.8, 0.55);
static const float3 RIM_COLOR = float3(0.35, 0.42, 0.52);

float4 main(PSInput input) : SV_Target0
{
    float3 normal = normalize(input.Normal);
    float3 lightDirection = normalize(LIGHT_DIRECTION);
    float3 viewDirection = normalize(CameraPos - input.WorldPos);

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 2.8);
    float3 litColor = (input.Color.rgb * (0.24 + diffuse * 0.92)) + (RIM_COLOR * rim * 0.35);

    return float4(litColor, input.Color.a);
}