struct ParticleVertex
{
    float4 PositionSize;    // xy = position, z = size
    float4 Color;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<ParticleVertex> particles;

[[vk::binding(1, 0)]]
cbuffer cbParams : register(b0)
{
    float4 SpawnCenterRadiusRate;
    float4 AngleSpeed;
    float4 LifetimeSize;
    float4 GravityDragTime;
    float4 BoundsMin;
    float4 BoundsMax;
    float4 ColorStart;
    float4 ColorEnd;
    float4 Meta;
};

float hash2(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453123);
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint id = dispatchThreadId.x;
    uint particleCount = (uint)Meta.x;
    if (id >= particleCount)
        return;

    float spawnRate = max(SpawnCenterRadiusRate.w, 0.001);
    float spawnInterval = 1.0 / spawnRate;
    float systemCycle = max((float)particleCount * spawnInterval, spawnInterval);
    float startOffset = (float)id * spawnInterval;
    float timeSinceFirstSpawn = GravityDragTime.w - startOffset;

    float4 deadVertex = float4(0.0, 0.0, 0.0, 0.0);
    particles[id].PositionSize = deadVertex;
    particles[id].Color = deadVertex;

    if (timeSinceFirstSpawn < 0.0)
        return;

    float cycleIndex = floor(timeSinceFirstSpawn / systemCycle);
    float age = timeSinceFirstSpawn - (cycleIndex * systemCycle);

    float2 seedBase = float2((float)id, cycleIndex + 1.0);
    float lifetimeRandom = hash2(seedBase + float2(1.37, 7.11));
    float lifetime = lerp(LifetimeSize.x, LifetimeSize.y, lifetimeRandom);
    if (age > lifetime)
        return;

    float angleRandom = hash2(seedBase + float2(3.17, 9.41));
    float speedRandom = hash2(seedBase + float2(5.23, 2.19));
    float radiusRandom = sqrt(hash2(seedBase + float2(8.63, 6.53)));
    float arcRandom = hash2(seedBase + float2(4.71, 1.29));

    float angle = lerp(AngleSpeed.x, AngleSpeed.y, angleRandom);
    float speed = lerp(AngleSpeed.z, AngleSpeed.w, speedRandom);
    float spawnAngle = arcRandom * 6.28318530718; // 2 * PI
    float radius = SpawnCenterRadiusRate.z * radiusRandom;

    float2 spawnPosition = SpawnCenterRadiusRate.xy + float2(cos(spawnAngle), sin(spawnAngle)) * radius;
    float2 initialVelocity = float2(cos(angle), sin(angle)) * speed;
    float2 gravity = GravityDragTime.xy * Meta.y;

    float2 position = spawnPosition + (initialVelocity * age) + (0.5 * gravity * age * age);

    if (position.x < BoundsMin.x || position.x > BoundsMax.x ||
        position.y < BoundsMin.y || position.y > BoundsMax.y)
        return;

    float lifeAlpha = clamp(age / max(lifetime, 0.001), 0.0, 1.0);
    float size = lerp(LifetimeSize.z, LifetimeSize.w, lifeAlpha);
    float4 color = lerp(ColorStart, ColorEnd, lifeAlpha);

    particles[id].PositionSize = float4(position, size, 1.0);
    particles[id].Color = color;
}