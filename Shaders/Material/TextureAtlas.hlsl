#ifndef MATERIAL_TEXTURE_ATLAS_HLSL
#define MATERIAL_TEXTURE_ATLAS_HLSL

// Discrete playback gives every cell an interval; blended playback spans frame centers.
float TextureAtlasFrameIndex(float phase, uint2 grid, bool interpolate)
{
    grid = max(grid, uint2(1, 1));
    const uint frames = grid.x * grid.y;
    const float span = interpolate ? float(frames - 1) : float(frames);
    return saturate(phase) * span;
}

float2 AtlasUV(float2 uv, uint frame, uint2 grid, uint2 imageSize)
{
    const uint2 cell = uint2(frame % grid.x, frame / grid.x);

    // Integer boundaries partition all texels, including grids that do not divide the image.
    const uint2 firstTexel = cell * imageSize / grid;
    const uint2 endTexel = (cell + 1) * imageSize / grid;
    const float2 position = lerp(float2(firstTexel), float2(endTexel), saturate(uv));

    // Keep the bilinear footprint inside this cell, even when it has only one texel.
    return clamp(position, float2(firstTexel) + 0.5, float2(endTexel) - 0.5) / float2(imageSize);
}

float4 SampleTextureAtlas(
    Texture2D image,
    SamplerState state,
    float2 uv,
    float frame,
    uint2 grid,
    bool interpolate
)
{
    grid = max(grid, uint2(1, 1));
    const uint frames = grid.x * grid.y;

    frame = isfinite(frame)
        ? clamp(frame, 0.0, float(frames - 1))
        : 0.0;

    uint first = uint(floor(frame));
    uint second = min(first + 1, frames - 1);

    uint width, height;
    image.GetDimensions(width, height);

    const uint2 imageSize = max(uint2(width, height), uint2(1, 1));

    // Explicit LOD prevents neighboring atlas cells from leaking through lower mip levels.
    float4 a = image.SampleLevel(state, AtlasUV(uv, first, grid, imageSize), 0);

    if (!interpolate)
        return a;

    float4 b = image.SampleLevel(state, AtlasUV(uv, second, grid, imageSize), 0);

    return lerp(a, b, frac(frame));
}

#endif
