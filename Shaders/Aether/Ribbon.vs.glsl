#version 450

struct ParticleState {
    vec4 PositionSize;  // xy = position, z = size, w = alive
    vec4 VelocityAge;   // xy = velocity, z = age, w = lifetime
    vec4 Color;
    vec4 Metadata;
};

layout(set = 0, binding = 0) uniform FrameData {
    mat4 ViewProj;
} cbFrame;

layout(set = 0, binding = 1, std430) readonly buffer ParticlesBuffer {
    ParticleState data[];
} particles;

layout(push_constant) uniform PushConstants {
    uint ParticleOffset;
    uint MaxParticles;
    uint HeadIndex;
    float WidthScale;
} pc;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;

const uint kCornerLUT[6] = uint[6](0u, 1u, 2u, 1u, 3u, 2u);

uint chronoToBuffer(uint chrono) {
    uint ringIndex = (pc.HeadIndex + pc.MaxParticles - chrono) % pc.MaxParticles;
    return pc.ParticleOffset + ringIndex;
}

void main() {
    uint vertexIndex = uint(gl_VertexIndex);
    uint segmentIndex = vertexIndex / 6u;
    uint cornerIndex = vertexIndex % 6u;
    uint corner = kCornerLUT[cornerIndex];

    uint i0 = chronoToBuffer(segmentIndex);
    uint i1 = chronoToBuffer(segmentIndex + 1u);

    ParticleState p0 = particles.data[i0];
    ParticleState p1 = particles.data[i1];

    vec2 pos0 = p0.PositionSize.xy;
    vec2 pos1 = p1.PositionSize.xy;
    float size0 = p0.PositionSize.z;
    float size1 = p1.PositionSize.z;

    vec2 delta = pos1 - pos0;
    float len = length(delta);
    vec2 tangent = (len > 1e-6) ? (delta / len) : vec2(1.0, 0.0);
    vec2 normal = vec2(-tangent.y, tangent.x);

    bool isTip = corner >= 2u;
    bool isRight = (corner & 1u) == 1u;
    float side = isRight ? 1.0 : -1.0;

    vec2 basePos = isTip ? pos1 : pos0;
    float baseSize = isTip ? size1 : size0;
    vec4 baseColor = isTip ? p1.Color : p0.Color;

    float halfWidth = baseSize * 0.5 * pc.WidthScale;
    vec2 worldPos = basePos + normal * halfWidth * side;

    bool alive0 = p0.PositionSize.w > 0.5;
    bool alive1 = p1.PositionSize.w > 0.5;
    if (!alive0 || !alive1) {
        baseColor.a = 0.0;
    }

    float along = (pc.MaxParticles > 1u)
        ? float(segmentIndex + (isTip ? 1u : 0u)) / float(pc.MaxParticles - 1u)
        : 0.0;

    gl_Position = cbFrame.ViewProj * vec4(worldPos, 0.0, 1.0);
    outColor = baseColor;
    outUV = vec2(along, isRight ? 1.0 : 0.0);
}
