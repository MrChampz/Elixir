#ifndef MATERIAL_SCENE_SAMPLING_HLSL
#define MATERIAL_SCENE_SAMPLING_HLSL
float4 SampleMaterialSceneColor(Texture2D image, SamplerState state, float2 pixel, float2 offset)
{
    uint width, height;
    image.GetDimensions(width, height);
    float2 dimensions = max(float2(width, height), float2(1, 1));
    float2 halfTexel = 0.5 / dimensions;
    float2 uv = clamp(pixel / dimensions + offset, halfTexel, 1.0 - halfTexel);
    return image.SampleLevel(state, uv, 0);
}
#endif
