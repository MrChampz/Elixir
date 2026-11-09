#include "../../../Shaders/Material/TextureAtlas.hlsl"

[[vk::binding(0, 0)]] Texture2D atlas;
[[vk::binding(1, 0)]] SamplerState linearClamp;
[[vk::binding(2, 0)]] RWStructuredBuffer<float4> results;

[numthreads(1, 1, 1)]
void main()
{
    results[0] = SampleTextureAtlas(atlas, linearClamp, float2(1, 1), 0, uint2(2, 2), 4, false);
    results[1] = SampleTextureAtlas(atlas, linearClamp, float2(0, 0), 1, uint2(2, 2), 4, false);
    results[2] = SampleTextureAtlas(atlas, linearClamp, float2(0.5, 0.5), 2, uint2(2, 2), 4, false);
    results[3] = SampleTextureAtlas(atlas, linearClamp, float2(0.5, 0.5), 3, uint2(2, 2), 4, false);
    results[4] = SampleTextureAtlas(atlas, linearClamp, float2(0.5, 0.5), 0.5, uint2(2, 2), 4, true);
    results[5] = SampleTextureAtlas(atlas, linearClamp, float2(0.5, 0.5), 0.5, uint2(2, 2), 4, false);
    results[6] = SampleTextureAtlas(atlas, linearClamp, float2(0.5, 0.5), 99, uint2(2, 2), 3, true);
    results[7] = SampleTextureAtlas(atlas, linearClamp, float2(0.5, 0.5), -1, uint2(2, 2), 4, true);
}
