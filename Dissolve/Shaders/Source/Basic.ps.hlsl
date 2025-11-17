struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    return float4(1.0, 0.0, 0.0, 1.0);
}