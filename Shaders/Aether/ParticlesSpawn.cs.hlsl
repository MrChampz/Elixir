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
    float4 MetaA; // x = offset in particle buffer, y = emitter count, z = module offset(spawn), w = module count(spawn)
    float4 MetaB; // x = module offset(update), y = module count(update), z = spawn cursor, w = spawn count
    float4 MetaC; // x = render mode
};

[[vk::binding(1, 0)]]
StructuredBuffer<Emitter> emitters;

struct Module
{
    float4 Header; // x = type, y = unused, z = parameter 0 index, w = parameter 1 index
    float4 Data0;
    float4 Data1;
    float4 Data2;
};

[[vk::binding(2, 0)]]
StructuredBuffer<Module> modules;

struct Parameter
{
    float4 Value;
};

[[vk::binding(3, 0)]]
StructuredBuffer<Parameter> parameters;

[[vk::binding(0, 1)]]
cbuffer cbParams : register(b0)
{
    float4 TimeData;
};

struct PushConstants {
    uint EmitterIndex;
};

[[vk::push_constant]]
PushConstants pc;

float Hash1(float x)
{
    return frac(sin(x * 91.3458 + 12.345) * 45678.5453);
}

float Hash2(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453123);
}

float4 ResolveValue(int parameterIndex, float4 fallbackValue)
{
    if (parameterIndex < 0)
        return fallbackValue;

    return parameters[parameterIndex].Value;
}

float2 CubicBezier(float2 p0, float2 p1, float2 p2, float2 p3, float t)
{
    float u = 1.0 - t;
    return (u * u * u * p0) +
        (3.0 * u * u * t * p1) +
        (3.0 * u * t * t * p2) +
        (t * t * t * p3);
}

bool InSpawnRange(uint localIndex, uint start, uint count, uint capacity)
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
    Emitter emitter = emitters[pc.EmitterIndex];
    uint localIndex = dispatchThreadId.x;
    uint emitterCount = (uint)emitter.MetaA.y;
    if (localIndex >= emitterCount)
        return;

    uint spawnCursor = (uint)emitter.MetaB.z;
    uint spawnCount = (uint)emitter.MetaB.w;
    if (!InSpawnRange(localIndex, spawnCursor, spawnCount, emitterCount))
        return;

    uint globalIndex = (uint)(emitter.MetaA.x + localIndex);
    float2 seedBase = float2(float(globalIndex), TimeData.y + float(spawnCursor));

    float2 position = float2(0.0, 0.0);
    float2 velocity = float2(0.0, 0.0);
    float4 color = float4(1.0, 1.0, 1.0, 1.0);
    float lifetime = 1.0;
    float size = 6.0;

    uint moduleOffset = (uint)emitter.MetaA.z;
    uint moduleCount = (uint)emitter.MetaA.w;

    for (uint i = 0u; i < moduleCount; ++i)
    {
        Module module = modules[moduleOffset + i];
        uint type = (uint)module.Header.x;

        int param0 = (int)module.Header.z;

        if (type == 1u) // SetPositionCircle
        {
            float radiusRandom = sqrt(Hash2(seedBase + float2(8.63, 6.53)));
            float arcRandom = Hash2(seedBase + float2(4.71, 1.29));
            float spawnAngle = arcRandom * 6.28318530718;
            float radius = module.Data0.z * radiusRandom;
            position = module.Data0.xy + float2(cos(spawnAngle), sin(spawnAngle)) * radius;
        }
        else if (type == 2u) // SetVelocityCone
        {
            float angleRandom = Hash2(seedBase + float2(3.17, 9.41));
            float speedRandom = Hash2(seedBase + float2(5.23, 2.19));
            float angle = lerp(module.Data0.x, module.Data0.y, angleRandom);
            float speed = lerp(module.Data0.z, module.Data0.w, speedRandom);
            velocity = float2(cos(angle), sin(angle)) * speed;
        }
        else if (type == 3u) // SetLifetime
        {
            float lifetimeRandom = Hash2(seedBase + float2(1.37, 7.11));
            lifetime = lerp(module.Data0.x, module.Data0.y, lifetimeRandom);
        }
        else if (type == 4u) // SetSize
        {
            size = module.Data0.x;
        }
        else if (type == 5u) // SetColor
        {
            color = ResolveValue(param0, module.Data0);
        }
        else if (type == 11u) // SetPositionOnCircle
        {
            float2 center = module.Data0.xy;
            float radius = module.Data0.z;
            float angularSpeed = module.Data0.w;
            float startAngle = module.Data1.x;
            float angle = TimeData.y * angularSpeed + startAngle;
            position = center + float2(cos(angle), sin(angle)) * radius;
        }
        else if (type == 12u) // SetPositionBezierLoop
        {
            float2 p0 = module.Data0.xy;
            float2 p1 = module.Data0.zw;
            float2 p2 = module.Data1.xy;
            float2 p3 = module.Data1.zw;
            float duration = max(module.Data2.x, 0.001);
            float startOffset = module.Data2.y;
            float t = frac((TimeData.y + startOffset) / duration);
            position = CubicBezier(p0, p1, p2, p3, t);
        }
    }

    particles[globalIndex].PositionSize = float4(position, size, 1.0);
    particles[globalIndex].VelocityAge = float4(velocity, 0.0, lifetime);
    particles[globalIndex].Color = color;
    particles[globalIndex].Metadata = float4(float(pc.EmitterIndex), Hash1(float(globalIndex)), 0.0, 0.0);
}
