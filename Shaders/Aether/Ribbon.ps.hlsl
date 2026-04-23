struct PSInput
{
    float4 ClipPos       : SV_POSITION;
    float4 Color         : COLOR;
    float2 UV            : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    float edgeFade = smoothstep(0.0, 0.24, input.UV.y) * smoothstep(1.0, 0.76, input.UV.y);
    float headGlow = 0.35 + (0.65 * pow(clamp(input.UV.x, 0.0, 1.0), 0.65));
    float3 color = input.Color.rgb + (float3(0.1, 0.08, 0.18) * headGlow);
    return float4(color, input.Color.a * edgeFade * headGlow);
}