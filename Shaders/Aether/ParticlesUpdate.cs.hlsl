struct ParticleState
{
    float4 PositionSize;    // xy = position, z = size, w = alive (0 or 1)
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
    float4 LifetimeSize;             // x = start lifetime, y = end lifetime, z = start size, w = end size
    float4 GravityDrag;              // xy = gravity, z = gravity scale, w = drag
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
    float4 TimeData;                  // x = delta time, y = total time, z = total particle count, w = max particle count
    Emitter Emitters[8];
};

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    uint totalParticleCount = (uint)TimeData.z;
    if (particleIndex >= totalParticleCount)
        return;

    ParticleState state = particles[particleIndex];
    if (state.PositionSize.w < 0.5)
    {
        particles[particleIndex].PositionSize.z = 0.0;
        particles[particleIndex].Color.a = 0.0;
        return;
    }

    uint emitterIndex = (uint)(state.Metadata.x + 0.5);
    Emitter emitter = Emitters[emitterIndex];
    float dt = TimeData.x;

    float age = state.VelocityAge.z + dt;
    float lifetime = max(state.VelocityAge.w, 0.001);
    float2 velocity = state.VelocityAge.xy + (emitter.GravityDrag.xy * emitter.GravityDrag.z * dt);
    velocity *= max(0.0, 1.0 - (emitter.GravityDrag.w * dt));
    float2 position = state.PositionSize.xy + (velocity * dt);

    bool outsideBounds = position.x < emitter.BoundsMin.x || position.x > emitter.BoundsMax.x ||
                         position.y < emitter.BoundsMin.y || position.y > emitter.BoundsMax.y;

    if (age >= lifetime || outsideBounds)
    {
        state.PositionSize = float4(position, 0.0, 0.0);
        state.VelocityAge = float4(0.0, 0.0, 0.0, 0.0);
        state.Color = float4(0.0, 0.0, 0.0, 0.0);

        particles[particleIndex] = state;
        return;
    }

    float lifeAlpha = clamp(age / lifetime, 0.0, 1.0);
    state.PositionSize = float4(position, lerp(emitter.LifetimeSize.z, emitter.LifetimeSize.w, lifeAlpha), 1.0);
    state.VelocityAge = float4(velocity, age, lifetime);
    state.Color = lerp(emitter.ColorStart, emitter.ColorEnd, lifeAlpha);

    particles[particleIndex] = state;
}