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
    uint ParticleCount;
    float WidthScale;
} pc;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;

const uint kCornerLUT[6] = uint[6](0u, 1u, 2u, 1u, 3u, 2u);

uint chronoToBuffer(uint chrono) {
    uint ringIndex = (pc.HeadIndex + pc.MaxParticles - chrono) % pc.MaxParticles;
    return pc.ParticleOffset + ringIndex;
}

vec2 safeNormalize(vec2 value, vec2 fallback) {
    float lengthSquared = dot(value, value);
    if (lengthSquared < 1e-6) {
        return fallback;
    }

    return value * inversesqrt(lengthSquared);
}

void main() {
    uint vertexIndex = uint(gl_VertexIndex);
    uint segmentIndex = vertexIndex / 6u;
    uint cornerIndex = vertexIndex % 6u;
    uint corner = kCornerLUT[cornerIndex];

    uint nextChrono = (segmentIndex > 0u) ? segmentIndex - 1u : 0u;
    uint prevChrono = (segmentIndex + 1u < pc.ParticleCount) ? segmentIndex + 1u : segmentIndex;

    uint prev = chronoToBuffer(prevChrono);
    uint curr = chronoToBuffer(segmentIndex);

    ParticleState p0 = particles.data[prev];
    ParticleState p1 = particles.data[curr];

    vec2 pos0 = p0.PositionSize.xy;
    vec2 pos1 = p1.PositionSize.xy;
    float size0 = p0.PositionSize.z;
    float size1 = p1.PositionSize.z;

    vec2 delta = pos1 - pos0;
    vec2 fallbackTangent = safeNormalize(delta, vec2(1.0, 0.0));
    vec2 tangent0 = safeNormalize(p0.Metadata.zw, fallbackTangent);
    vec2 tangent1 = safeNormalize(p1.Metadata.zw, fallbackTangent);
    vec2 normal0 = vec2(-tangent0.y, tangent0.x);
    vec2 normal1 = vec2(-tangent1.y, tangent1.x);

    bool isTip = corner >= 2u;
    bool isRight = (corner & 1u) == 1u;
    float side = isRight ? 1.0 : -1.0;

    vec2 basePos = isTip ? pos1 : pos0;
    float baseSize = isTip ? size1 : size0;
    vec4 baseColor = isTip ? p1.Color : p0.Color;
    vec2 baseNormal = isTip ? normal1 : normal0;

    float halfWidth = baseSize * 0.5 * pc.WidthScale;
    vec2 worldPos = basePos + baseNormal * halfWidth * side;

    bool alive0 = p0.PositionSize.w > 0.5;
    bool alive1 = p1.PositionSize.w > 0.5;
    if (!alive0 || !alive1) {
        baseColor.a = 0.0;
    }

    float along = (pc.ParticleCount > 1u)
        ? float(segmentIndex + (isTip ? 1u : 0u)) / float(pc.ParticleCount - 1u)
        : 0.0;

    gl_Position = cbFrame.ViewProj * vec4(worldPos, 0.0, 1.0);
    outColor = baseColor;
    outUV = vec2(along, isRight ? 1.0 : 0.0);
}
