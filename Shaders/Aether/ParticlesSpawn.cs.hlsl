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

struct AttributeTable
{
    float4 Position;
    float4 Velocity;
    float4 Color;
    float4 Size;
    float4 Lifetime;
    float4 Tangent;
};

[[vk::binding(0, 1)]]
cbuffer cbParams : register(b0)
{
    float4 TimeData;
    float4 ViewportData;
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

float4 GetAttribute(AttributeTable table, uint attrId)
{
    if (attrId == 1u)
        return table.Position;
    if (attrId == 2u)
        return table.Velocity;
    if (attrId == 3u)
        return table.Color;
    if (attrId == 4u)
        return table.Size;
    if (attrId == 5u)
        return table.Lifetime;
    if (attrId == 6u)
        return table.Tangent;

    return float4(0.0, 0.0, 0.0, 0.0);
}

void SetAttribute(inout AttributeTable table, uint attrId, float4 value)
{
    if (attrId == 1u)
        table.Position = value;
    else if (attrId == 2u)
        table.Velocity = value;
    else if (attrId == 3u)
        table.Color = value;
    else if (attrId == 4u)
        table.Size = value;
    else if (attrId == 5u)
        table.Lifetime = value;
    else if (attrId == 6u)
        table.Tangent = value;
}

float2 RibbonPath(float timeSeconds)
{
    float phase = timeSeconds * 1.08;
    float x = (0.56 * sin(phase)) + (0.14 * sin(phase * 2.1));
    float y = (-0.18 + (0.38 * sin((phase * 0.58) + 0.65))) + (0.12 * cos((phase * 1.34) - 0.4));
    return float2(x, y);
}

float2 CubicBezier(float2 p0, float2 p1, float2 p2, float2 p3, float t)
{
    float u = 1.0 - t;
    return (u * u * u * p0) +
        (3.0 * u * u * t * p1) +
        (3.0 * u * t * t * p2) +
        (t * t * t * p3);
}

float2 CubicBezierDerivative(float2 p0, float2 p1, float2 p2, float2 p3, float t)
{
    float u = 1.0 - t;
    return (3.0 * u * u * (p1 - p0)) +
        (6.0 * u * t * (p2 - p1)) +
        (3.0 * t * t * (p3 - p2));
}

float BezierArcLength(float2 p0, float2 p1, float2 p2, float2 p3)
{
    static const uint steps = 32u;
    float2 previous = p0;
    float length = 0.0;

    [unroll]
    for (uint i = 1u; i <= steps; ++i)
    {
        float t = float(i) / float(steps);
        float2 current = CubicBezier(p0, p1, p2, p3, t);
        length += distance(previous, current);
        previous = current;
    }

    return length;
}

float BezierTAtArcLength(float2 p0, float2 p1, float2 p2, float2 p3, float normalizedDistance)
{
    static const uint steps = 32u;
    float totalLength = max(BezierArcLength(p0, p1, p2, p3), 0.000001);
    float targetLength = frac(normalizedDistance) * totalLength;
    float walkedLength = 0.0;
    float2 previous = p0;

    [unroll]
    for (uint i = 1u; i <= steps; ++i)
    {
        float previousT = float(i - 1u) / float(steps);
        float currentT = float(i) / float(steps);
        float2 current = CubicBezier(p0, p1, p2, p3, currentT);
        float segmentLength = distance(previous, current);

        if (walkedLength + segmentLength >= targetLength)
        {
            float segmentT = (targetLength - walkedLength) / max(segmentLength, 0.000001);
            return lerp(previousT, currentT, saturate(segmentT));
        }

        walkedLength += segmentLength;
        previous = current;
    }

    return 1.0;
}

float2 SafeNormalize(float2 value, float2 fallback)
{
    float lengthSquared = dot(value, value);
    if (lengthSquared < 0.000001)
        return fallback;

    return value * rsqrt(lengthSquared);
}

bool InLoopSegment(float phase, float segmentStart, float segmentLength, out float segmentPhase)
{
    float length = max(segmentLength, 0.000001);
    float start = frac(segmentStart);
    float delta = frac(phase - start + 1.0);
    segmentPhase = saturate(delta / length);
    return delta < length;
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

    AttributeTable attributes;
    attributes.Position = float4(0.0, 0.0, 0.0, 0.0);
    attributes.Velocity = float4(0.0, 0.0, 0.0, 0.0);
    attributes.Color = float4(1.0, 1.0, 1.0, 1.0);
    attributes.Size = float4(6.0, 0.0, 0.0, 0.0);
    attributes.Lifetime = float4(1.0, 0.0, 0.0, 0.0);
    attributes.Tangent = float4(1.0, 0.0, 0.0, 0.0);

    uint moduleOffset = (uint)emitter.MetaA.z;
    uint moduleCount = (uint)emitter.MetaA.w;
    bool isRibbon = ((uint)(emitter.MetaC.x + 0.5)) == 1u;

    for (uint i = 0u; i < moduleCount; ++i)
    {
        Module module = modules[moduleOffset + i];
        uint type = (uint)module.Header.x;
        uint target = (uint)module.Header.y;

        int param0 = (int)module.Header.z;

        if (type == 1u) // SetPositionCircle
        {
            float radiusRandom = sqrt(Hash2(seedBase + float2(8.63, 6.53)));
            float arcRandom = Hash2(seedBase + float2(4.71, 1.29));
            float spawnAngle = arcRandom * 6.28318530718;
            float radius = module.Data0.z * radiusRandom;
            float2 position = module.Data0.xy + float2(cos(spawnAngle), sin(spawnAngle)) * radius;
            SetAttribute(attributes, target, float4(position, 0.0, 0.0));

            float2 tangent = float2(-sin(spawnAngle), cos(spawnAngle));
            SetAttribute(attributes, 6u, float4(tangent, 0.0, 0.0));
        }
        else if (type == 2u) // SetVelocityCone
        {
            float angleRandom = Hash2(seedBase + float2(3.17, 9.41));
            float speedRandom = Hash2(seedBase + float2(5.23, 2.19));
            float angle = lerp(module.Data0.x, module.Data0.y, angleRandom);
            float speed = lerp(module.Data0.z, module.Data0.w, speedRandom);
            float2 velocity = float2(cos(angle), sin(angle)) * speed;
            SetAttribute(attributes, target, float4(velocity, 0.0, 0.0));

            float2 tangent = SafeNormalize(velocity, GetAttribute(attributes, 6u).xy);
            SetAttribute(attributes, 6u, float4(tangent, 0.0, 0.0));
        }
        else if (type == 3u) // SetLifetime
        {
            float lifetimeRandom = Hash2(seedBase + float2(1.37, 7.11));
            float lifetime = lerp(module.Data0.x, module.Data0.y, lifetimeRandom);
            SetAttribute(attributes, target, float4(lifetime, 0.0, 0.0, 0.0));
        }
        else if (type == 4u) // SetSize
        {
            float size = module.Data0.x;
            SetAttribute(attributes, target, float4(size, 0.0, 0.0, 0.0));
        }
        else if (type == 5u) // SetColor
        {
            float4 color = ResolveValue(param0, module.Data0);
            SetAttribute(attributes, target, color);
        }
        else if (type == 11u) // SetPositionOnCircle
        {
            float2 center = module.Data0.xy;
            float radius = module.Data0.z;
            float angularSpeed = module.Data0.w;
            float startAngle = module.Data1.x;
            float angle = TimeData.y * angularSpeed + startAngle;

            float2 position = center + float2(cos(angle), sin(angle)) * radius;
            SetAttribute(attributes, target, float4(position, 0.0, 1.0));

            float orientation = angularSpeed < 0.0 ? -1.0 : 1.0;
            float2 tangent = float2(-sin(angle), cos(angle)) * orientation;
            SetAttribute(attributes, 6u, float4(tangent, 0.0, 0.0));

            SetAttribute(attributes, 2u, float4(0.0, 0.0, 0.0, 0.0));
        }
    }

    //uint renderMode = (uint)(emitter.MetaC.x + 0.5);
    //if (renderMode == 1u) // Ribbon
    //{
    //    float spawnRate = max(emitter.MetaC.y, 1.0);
    //    uint spawnOrder = SpawnOrderInBatch(localIndex, spawnCursor, emitterCount);
    //    float timeOffset = float((spawnCount - 1u) - spawnOrder) / spawnRate;
    //    float2 pathPosition = RibbonPath(TimeData.y - timeOffset);
    //    SetAttribute(attributes, 1u, float4(pathPosition, 0.0, 1.0));
    //    SetAttribute(attributes, 2u, float4(0.0, 0.0, 0.0, 0.0));
    //}

    particles[globalIndex].PositionSize = float4(GetAttribute(attributes, 1u).xy, GetAttribute(attributes, 4u).x, 1.0);
    particles[globalIndex].VelocityAge = float4(GetAttribute(attributes, 2u).xy, 0.0, GetAttribute(attributes, 5u).x);
    particles[globalIndex].Color = GetAttribute(attributes, 3u);
    particles[globalIndex].Metadata = float4(float(pc.EmitterIndex), Hash1(float(globalIndex)), GetAttribute(attributes, 6u).xy);

    if (isRibbon)
    {
        particles[globalIndex].PositionSize.w = 2.0; // Immortal particle
    }
}