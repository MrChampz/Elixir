#version 450

struct ParticleState {
    vec4 PositionSize;  // xyz = position, w = size
    vec4 VelocityAge;   // xyz = velocity, w = age
    vec4 Tangent;       // xyz = tangent, w = unused
    vec4 Color;
    vec4 Metadata;      // x = emitter index, y = random seed, z = lifetime, w = alive
};

layout(set = 0, binding = 0) uniform FrameData {
    mat4 View;
    mat4 Proj;
    mat4 ViewProj;
} cbFrame;

layout(set = 0, binding = 1, std430) readonly buffer ParticleBuffer {
    ParticleState data[];
} particles;

layout(push_constant) uniform PushConstants {
    uint ParticleOffset;
    uint MaxParticles;
    uint StartIndex;
    uint ParticleCount;
    float WidthScale;
    float ViewportWidth;
    float ViewportHeight;
} pc;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;

const uint kCornerLUT[6] = uint[6](0u, 1u, 2u, 1u, 3u, 2u);

uint chronoToBuffer(uint chrono) {
    uint ringIndex = (pc.StartIndex + pc.MaxParticles - chrono) % pc.MaxParticles;
    return pc.ParticleOffset + ringIndex;
}

vec2 safeNormalize2(vec2 value, vec2 fallback) {
    float lengthSquared = dot(value, value);
    if (lengthSquared < 1e-6) {
        return fallback;
    }

    return value * inversesqrt(lengthSquared);
}

vec4 getClipPosition(uint chrono) {
    ParticleState particle = particles.data[chronoToBuffer(chrono)];
    return cbFrame.ViewProj * vec4(particle.PositionSize.xyz, 1.0);
}

vec2 clipToPixel(vec4 clipPosition) {
    vec2 viewportSize = max(vec2(pc.ViewportWidth, pc.ViewportHeight), vec2(1.0));
    vec2 ndc = clipPosition.xy / clipPosition.w;
    return (ndc * 0.5 + 0.5) * viewportSize;
}

vec2 getScreenPixel(uint chrono) {
    return clipToPixel(getClipPosition(chrono));
}

vec2 segmentTangent(vec2 from, vec2 to) {
    return safeNormalize2(to - from, vec2(1.0, 0.0));
}

vec2 pointTangent(uint chrono) {
    if (pc.ParticleCount < 2u) {
        return vec2(1.0, 0.0);
    }

    vec2 curr = getScreenPixel(chrono);

    if (chrono == 0u) {
        return segmentTangent(getScreenPixel(1u), curr);
    }

    if (chrono + 1u >= pc.ParticleCount) {
        return segmentTangent(curr, getScreenPixel(chrono - 1u));
    }

    vec2 inTangent = segmentTangent(getScreenPixel(chrono + 1u), curr);
    vec2 outTangent = segmentTangent(curr, getScreenPixel(chrono - 1u));
    return safeNormalize2(inTangent + outTangent, outTangent);
}

vec2 normalFromTangent(vec2 tangent) {
    return vec2(-tangent.y, tangent.x);
}

void main() {
    uint vertexIndex = uint(gl_VertexIndex);
    uint segmentIndex = vertexIndex / 6u;
    uint cornerIndex = vertexIndex % 6u;
    uint corner = kCornerLUT[cornerIndex];

    uint prevChrono = (segmentIndex + 1u < pc.ParticleCount) ? segmentIndex + 1u : segmentIndex;

    uint prev = chronoToBuffer(prevChrono);
    uint curr = chronoToBuffer(segmentIndex);

    ParticleState p0 = particles.data[prev];
    ParticleState p1 = particles.data[curr];

    vec3 pos0 = p0.PositionSize.xyz;
    vec3 pos1 = p1.PositionSize.xyz;
    float size0 = p0.PositionSize.w;
    float size1 = p1.PositionSize.w;

    vec4 clipPos0 = cbFrame.ViewProj * vec4(pos0, 1.0);
    vec4 clipPos1 = cbFrame.ViewProj * vec4(pos1, 1.0);

    vec2 normal0 = normalFromTangent(pointTangent(prevChrono));
    vec2 normal1 = normalFromTangent(pointTangent(segmentIndex));

    bool isTip = corner >= 2u;
    bool isRight = (corner & 1u) == 1u;
    float side = isRight ? 1.0 : -1.0;

    vec4 baseClipPos = isTip ? clipPos1 : clipPos0;
    float baseSize = isTip ? size1 : size0;
    vec4 baseColor = isTip ? p1.Color : p0.Color;
    vec2 baseNormal = isTip ? normal1 : normal0;

    float halfWidthPixels = baseSize * 0.5 * pc.WidthScale;
    vec2 viewportSize = max(vec2(pc.ViewportWidth, pc.ViewportHeight), vec2(1.0));
    vec2 ndcOffset = (baseNormal * halfWidthPixels * side / viewportSize) * 2.0;
    vec4 clipPos = baseClipPos;
    clipPos.xy += ndcOffset * clipPos.w;

    bool alive0 = p0.Metadata.w > 0.5;
    bool alive1 = p1.Metadata.w > 0.5;
    if (!alive0 || !alive1) {
        baseColor.a = 0.0;
    }

    float chrono = isTip ? float(segmentIndex) : float(prevChrono);
    float along = (pc.ParticleCount > 1u)
        ? chrono / float(pc.ParticleCount - 1u)
        : 0.0;

    gl_Position = clipPos;
    outColor = baseColor;
    outUV = vec2(along, isRight ? 1.0 : 0.0);
}
