#ifndef MATERIAL_TEXTURE_ATLAS_HLSL
#define MATERIAL_TEXTURE_ATLAS_HLSL

float2 AtlasUV(float2 uv, uint frame, uint2 grid, float2 halfTexel)
{
    float2 minimum = float2(frame % grid.x, frame / grid.x) / float2(grid);
    float2 cellSize = 1.0 / float2(grid);
    halfTexel = min(halfTexel, cellSize * 0.5);

    return clamp(
        minimum + saturate(uv) * cellSize,
        minimum + halfTexel,
        minimum + cellSize - halfTexel
    );
}

float4 SampleTextureAtlas(
    Texture2D image,
    SamplerState state,
    float2 uv,
    float frame,
    uint2 grid,
    uint frames,
    bool interpolate
)
{
    grid = max(grid, uint2(1, 1));
    frames = max(frames, 1u);

    frame = isfinite(frame)
        ? clamp(frame, 0.0, float(frames - 1))
        : 0.0;

    uint first = uint(floor(frame));
    uint second = min(first + 1, frames - 1);

    uint width, height;
    image.GetDimensions(width, height);

    float2 halfTexel = 0.5 / max(float2(width, height), float2(1, 1));

    // Explicit LOD prevents neighboring atlas cells from leaking through lower mip levels.
    float4 a = image.SampleLevel(state, AtlasUV(uv, first, grid, halfTexel), 0);

    if (!interpolate)
        return a;

    float4 b = image.SampleLevel(state, AtlasUV(uv, second, grid, halfTexel), 0);

    return lerp(a, b, frac(frame));
}

#endif
