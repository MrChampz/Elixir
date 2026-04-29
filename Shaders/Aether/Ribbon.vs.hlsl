struct ParticleState
{
    float4 PositionSize;    // xy = position, z = size, w = alive
    float4 VelocityAge;     // xy = velocity, z = age, w = lifetime
    float4 Color;
    float4 Metadata;        // x = emitter index, y = random seed
};

[[vk::binding(0, 0)]]
StructuredBuffer<ParticleState> particles;

[[vk::binding(0, 1)]]
cbuffer cbFrame : register(b0)
{
    float4x4 ViewProj;
}

[[vk::binding(1, 1)]]
cbuffer cbParams : register(b1)
{
    float4 TimeData;
    float4 ViewportData;
};

struct PushConstants {
    uint ParticleOffset;
    uint MaxParticles;
    uint HeadIndex;
    float WidthScale;
};

[[vk::push_constant]]
PushConstants pc;

struct VSOutput
{
    float4 ClipPos       : SV_POSITION;
    float4 Color         : COLOR;
    float2 UV            : TEXCOORD0;
};

uint ChronoToBuffer(uint chrono)
{
    uint capacity = max(pc.MaxParticles, 1u);
    uint ringIndex = (pc.HeadIndex + capacity - chrono) % capacity;
    return pc.ParticleOffset + ringIndex;
}

ParticleState LoadChrono(uint chrono)
{
    return particles[ChronoToBuffer(chrono)];
}

float2 GetSafeDirection(float2 direction)
{
    float lengthSquared = dot(direction, direction);
    if (lengthSquared < 0.000001)
        return float2(0.0, 1.0);

    return direction * rsqrt(lengthSquared);
}

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;

    uint segmentIndex = vertexId / 6u;
    uint cornerIndex = vertexId % 6u;
    uint maxParticles = max(pc.MaxParticles, 1u);

    const uint chronos[6] = { segmentIndex, segmentIndex, segmentIndex + 1u, segmentIndex, segmentIndex + 1u, segmentIndex + 1u };
    const float sides[6] = { -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f };
    const float longitudinal[6] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f };

    uint chrono = min(chronos[cornerIndex], maxParticles - 1u);
    ParticleState current = LoadChrono(chrono);
    uint nextChrono = min(chrono + 1u, maxParticles - 1u);
    ParticleState next = LoadChrono(nextChrono);

    float2 currentPosition = current.PositionSize.xy;
    float2 nextPosition = next.PositionSize.xy;

    // Tangent points from older to newer (head direction); for chrono ordering newer = lower chrono.
    float2 tangent = GetSafeDirection(currentPosition - nextPosition);
    float2 normal = float2(-tangent.y, tangent.x);

    float side = sides[cornerIndex];
    float along = longitudinal[cornerIndex];

    float widthPixels = lerp(current.PositionSize.z, next.PositionSize.z, along) * pc.WidthScale;
    float width = (widthPixels * 2.0f) / max(ViewportData.y, 1.0);
    float alive = current.PositionSize.w >= 0.5 && next.PositionSize.w >= 0.5 ? 1.0f : 0.0f;

    float2 centerPosition = lerp(currentPosition, nextPosition, along);
    float2 ribbonPosition = centerPosition + (normal * width * side);

    float4 pos = float4(ribbonPosition, 0.0f, 1.0f);
    output.ClipPos = mul(ViewProj, pos);

    float4 color = lerp(current.Color, next.Color, along);
    output.Color = float4(color.rgb, color.a * alive);

    float2 uv = float2(
        maxParticles > 1u ? ((float)chrono / (float)(maxParticles - 1u)) : 0.0f,
        side > 0.0 ? 1.0 : 0.0
    );
    output.UV = uv;

    return output;
}
