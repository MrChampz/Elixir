#include "../Quad.hlsl"

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float _Padding;
};

struct SpritePushConstants
{
    uint SpriteIndex;
    uint FlipCols;      // atlas columns (1 = no flipbook)
    uint FlipRows;      // atlas rows
    uint FlipFrames;    // active frame count (<= cols * rows)
    uint FlipBlend;     // 0 = hard frame steps, 1 = cross-fade adjacent frames
    uint GradientIndex; // LUT texture index (pixel stage only)
    uint GradientMode;  // 0 = off, 1 = luminance -> LUT remap
    uint NormalIndex;   // fake-normal map index (pixel stage only)
    uint NormalMode;    // 0 = unlit, 1 = fake-normal lighting
    uint EmissionIndex; // emission map index (pixel stage only)
    float EmissionScale; // 0 = no emission map
};
[[vk::push_constant]]
SpritePushConstants pc;

struct VSInput
{
    float4 PositionSize    : POSITION;
    float4 VelocityAge     : TEXCOORD0;
    float4 Transform       : TEXCOORD1;
    float4 TangentRibbonId : TEXCOORD2;
    float4 Color           : COLOR;
    float4 Metadata        : TEXCOORD3; // x = emitter, z = lifetime, w = alive
};

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
    float4 Color   : COLOR;
    float2 CellA   : TEXCOORD0; // (col,row) of current frame
    float2 CellB   : TEXCOORD1; // (col,row) of next frame
    float2 QuadUV  : TEXCOORD2; // within-cell 0..1 coords
    float  Blend   : TEXCOORD3; // 0..1 weight toward frame B
};

// Linear frame index -> atlas (col,row).
float2 FrameCell(uint frame, uint cols)
{
    return float2((float)(frame % cols), (float)(frame / cols));
}

VSOutput main(VSInput input, uint vertexId : SV_VertexID)
{
    VSOutput output;

    // Generate quad positions using bit manipulation
    float2 normalizedPos = CalculateQuadPosition(vertexId % 6);

    float size = input.PositionSize.w * max(input.Transform.y, 0.0f);

    float3 viewPos = mul(View, float4(input.PositionSize.xyz, 1.0f)).xyz;
    viewPos.xy += (normalizedPos - 0.5f) * size; // Center around origin

    output.ClipPos = mul(Proj, float4(viewPos, 1.0f));
    output.Color = input.Color;

    uint cols = max(pc.FlipCols, 1u);
    uint frames = max(pc.FlipFrames, 1u);

    // Frame progresses across the particle's lifetime (play-once). Age and
    // lifetime already ride along in the instance data, so no compute-side
    // state is required to drive the animation.
    float age = input.VelocityAge.w;
    float lifetime = max(input.Metadata.z, 1e-4f);
    float life = saturate(age / lifetime);

    float frameF = life * (float)frames;
    uint frame0 = min((uint)floor(frameF), frames - 1u);
    uint frame1 = min(frame0 + 1u, frames - 1u);

    // Hand the cell coordinates and within-cell UV to the pixel stage, which
    // insets the sample to the cell interior so bilinear taps never bleed a
    // neighbouring frame across the atlas seams.
    output.CellA = FrameCell(frame0, cols);
    output.CellB = FrameCell(frame1, cols);
    output.QuadUV = normalizedPos;
    output.Blend = (pc.FlipBlend != 0u) ? frac(frameF) : 0.0f;

    return output;
}
