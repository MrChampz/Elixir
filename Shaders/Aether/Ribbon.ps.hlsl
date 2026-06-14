struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
    nointerpolation float Valid : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target0
{
    clip(input.Valid - 0.5);

    float centeredAcrossRibbon = abs((input.UV.x * 2.0) - 1.0);
    float edgeFade = 1.0 - smoothstep(0.72, 1.0, centeredAcrossRibbon);
    float coreGlow = 1.0 - smoothstep(0.0, 0.52, centeredAcrossRibbon);
    float3 color = input.Color.rgb + (coreGlow * 0.22);

    return float4(color, input.Color.a * edgeFade);
}