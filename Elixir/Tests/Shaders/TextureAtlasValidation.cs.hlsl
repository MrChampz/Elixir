#include "../../../Shaders/Material/TextureAtlas.hlsl"

[[vk::binding(0, 0)]] Texture2D atlas;
[[vk::binding(1, 0)]] SamplerState linearClamp;
[[vk::binding(2, 0)]] RWStructuredBuffer<float4> results;

[numthreads(1, 1, 1)]
void main()
{
    results[0] = SampleTextureAtlas(atlas, linearClamp, float2(1, 1), 0, uint2(2, 2), false);
    results[1] = SampleTextureAtlas(atlas, linearClamp, float2(0, 0), 1, uint2(2, 2), false);
    results[2] = SampleTextureAtlas(atlas, linearClamp, float2(0.5, 0.5), 2, uint2(2, 2), false);
    results[3] = SampleTextureAtlas(atlas, linearClamp, float2(0.5, 0.5), 3, uint2(2, 2), false);
    results[4] = SampleTextureAtlas(atlas, linearClamp, float2(0.5, 0.5), 0.5, uint2(2, 2), true);
    results[5] = SampleTextureAtlas(atlas, linearClamp, float2(0.5, 0.5), 0.5, uint2(2, 2), false);
    results[6] = SampleTextureAtlas(atlas, linearClamp, float2(0.5, 0.5), 99, uint2(2, 2), true);
    results[7] = SampleTextureAtlas(atlas, linearClamp, float2(0.5, 0.5), -1, uint2(2, 2), true);
    const float phases[6] = {0.0, 0.249, 0.25, 0.5, 0.75, 0.999};
    for (uint index = 0; index < 6; ++index)
    {
        const float frame = TextureAtlasFrameIndex(phases[index], uint2(2, 2), false);
        results[8 + index] = SampleTextureAtlas(
            atlas, linearClamp, float2(0.5, 0.5), frame, uint2(2, 2), false
        );
    }
    const float blended = TextureAtlasFrameIndex(0.5, uint2(2, 2), true);
    results[14] = SampleTextureAtlas(
        atlas, linearClamp, float2(0.5, 0.5), blended, uint2(2, 2), true
    );
    const float end = TextureAtlasFrameIndex(1.0, uint2(2, 2), false);
    results[15] = SampleTextureAtlas(
        atlas, linearClamp, float2(0.5, 0.5), end, uint2(2, 2), false
    );
}
