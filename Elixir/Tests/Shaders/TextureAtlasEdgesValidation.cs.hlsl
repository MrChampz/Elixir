#include "../../../Shaders/Material/TextureAtlas.hlsl"

[[vk::binding(0, 0)]] Texture2D atlas;
[[vk::binding(1, 0)]] SamplerState linearClamp;
[[vk::binding(2, 0)]] RWStructuredBuffer<float4> results;

[numthreads(1, 1, 1)]
void main()
{
    uint width, height;
    atlas.GetDimensions(width, height);
    const uint2 grid = uint2(width == 4 ? 3 : 10, 3);
    const uint frames = grid.x * grid.y;
    const float2 coordinates[5] = {
        float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 1), float2(0.5, 0.5)
    };
    for (uint frame = 0; frame < frames; ++frame)
    {
        for (uint sample = 0; sample < 5; ++sample)
        {
            results[frame * 6 + sample] = SampleTextureAtlas(
                atlas, linearClamp, coordinates[sample], float(frame), grid, false
            );
        }
        results[frame * 6 + 5] = SampleTextureAtlas(
            atlas, linearClamp, float2(1, 1), float(frame) + 0.5, grid, true
        );
    }
}
