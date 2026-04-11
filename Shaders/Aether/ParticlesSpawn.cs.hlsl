struct ParticleState
{
    float4 PositionSize;    // xy = position, z = size
    float4 VelocityAge;     // xy = velocity, z = age, w = lifetime
    float4 Color;
    float4 Metadata;        // x = emitter index, y = random seed
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<ParticleState> particles;

struct Emitter
{
    float4 SpawnCenterRadiusRate;
    float4 AngleSpeed;
    float4 LifetimeSize;
    float4 GravityDrag;
    float4 BoundsMin;
    float4 BoundsMax;
    float4 ColorStart;
    float4 ColorEnd;
    float4 MetaA;                    // x = offset in particle buffer, y = max particles in emitter
    float4 MetaB;                    // x = spawn cursor, y = spawn count
};

[[vk::binding(1, 0)]]
cbuffer cbParams : register(b0)
{
    float4 TimeData;
    Emitter Emitters[8];
};

struct PushConstants {
    uint EmitterIndex;
};

[[vk::push_constant]]
PushConstants pc;

float hash1(float x)
{
    return frac(sin(x * 91.3458 + 12.345) * 45678.5453);
}

float hash2(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453123);
}

/**
 * Determines if a given local index falls within the spawn range defined by start and count,
 * considering wrap-around.
 *
 * @param localIndex The index to check.
 * @param start The starting index of the spawn range.
 * @param count The number of indices in the spawn range.
 * @param capacity The total capacity of the buffer (used for wrap-around).
 * @return true if localIndex is within the spawn range, false otherwise.
 */
bool inSpawnRange(uint localIndex, uint start, uint count, uint capacity)
{
    if (count == 0)
        return false;

    if (count >= capacity)
        return true;

    uint end = start + count;
    if (end <= capacity) {
        return localIndex >= start && localIndex < end;
    }

    uint wrappedEnd = end - capacity;
    return localIndex >= start || localIndex < wrappedEnd;
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    Emitter emitter = Emitters[pc.EmitterIndex];
    uint localIndex = dispatchThreadId.x;
    uint emitterCount = (uint)emitter.MetaA.y;
    if (localIndex >= emitterCount)
        return;

    uint spawnCursor = (uint)emitter.MetaB.x;
    uint spawnCount = (uint)emitter.MetaB.y;
    if (!inSpawnRange(localIndex, spawnCursor, spawnCount, emitterCount))
        return;

    uint globalIndex = (uint)emitter.MetaA.x + localIndex;
    float2 seedBase = float2(float(globalIndex), TimeData.y + float(spawnCursor));

    float angleRandom = hash2(seedBase + float2(3.17, 9.41));
    float speedRandom = hash2(seedBase + float2(5.23, 2.19));
    float radiusRandom = sqrt(hash2(seedBase + float2(8.63, 6.33)));
    float arcRandom = hash2(seedBase + float2(4.71, 1.29));
    float lifetimeRandom = hash2(seedBase + float2(1.37, 7.11));

    float angle = lerp(emitter.AngleSpeed.x, emitter.AngleSpeed.y, angleRandom);
    float speed = lerp(emitter.AngleSpeed.z, emitter.AngleSpeed.w, speedRandom);
    float spawnAngle = arcRandom * 6.28318530718; // 2 * PI
    float radius = emitter.SpawnCenterRadiusRate.z * radiusRandom;
    float lifetime = lerp(emitter.LifetimeSize.x, emitter.LifetimeSize.y, lifetimeRandom);

    float2 spawnPosition = emitter.SpawnCenterRadiusRate.xy + float2(cos(spawnAngle), sin(spawnAngle)) * radius;
    float2 initialVelocity = float2(cos(angle), sin(angle)) * speed;

    particles[globalIndex].PositionSize = float4(spawnPosition, emitter.LifetimeSize.z, 1.0);
    particles[globalIndex].VelocityAge = float4(initialVelocity, 0.0, lifetime);
    particles[globalIndex].Color = emitter.ColorStart;
    particles[globalIndex].Metadata = float4(float(pc.EmitterIndex), hash1(float(globalIndex)), 0.0, 0.0);
}