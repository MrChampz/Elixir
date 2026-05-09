struct ParticleState
{
    float4 PositionSize;    // xyz = position, w = size
    float4 VelocityAge;     // xyz = velocity, w = age
    float4 Tangent;         // xyz = tangent, w = ribbon id
    float4 Color;
    float4 Metadata;        // x = emitter index, y = random seed, z = lifetime, w = alive
};

struct Emitter
{
    float4 MetaA; // x = offset, y = max particles, z = spawn module offset, w = spawn module count
    float4 MetaB; // x = update module offset, y = update module count, z = spawn cursor, w = spawn count
    float4 MetaC; // x = render mode, y = spawn rate, z = gravity scale, w = next buffer cursor
    float4 MetaD; // x = spawn serial at the start of this dispatch
};

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 ViewProj;
    float3 CameraPosition;
    float _FramePadding;
};

[[vk::binding(1, 0)]]
StructuredBuffer<ParticleState> particles;

[[vk::binding(2, 0)]]
StructuredBuffer<Emitter> emitters;

struct PushConstants
{
    uint EmitterIndex;
};

[[vk::push_constant]]
PushConstants pc;

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV : TEXCOORD0;
};

static const float RIBBON_WORLD_SIZE_SCALE = 0.01;

float3 SafeNormalize(float3 value, float3 fallback)
{
    float lengthSquared = dot(value, value);
    if (lengthSquared < 0.000001)
        return fallback;

    return value * rsqrt(lengthSquared);
}

bool IsAlive(ParticleState particle)
{
    return particle.Metadata.w >= 0.5 &&
        particle.Color.a > 0.0001 &&
        particle.PositionSize.w > 0.0001;
}

bool SameRibbon(float a, float b)
{
    return abs(a - b) < 0.5;
}

bool TryBuildSegment(
    uint localIndex,
    Emitter emitter,
    out ParticleState startParticle,
    out ParticleState endParticle,
    out uint endLocalIndex
)
{
    uint particleOffset = (uint)emitter.MetaA.x;
    uint particleCount = (uint)emitter.MetaA.y;
    uint nextCursor = (uint)emitter.MetaC.w;

    startParticle = particles[particleOffset + localIndex];
    endParticle = startParticle;
    endLocalIndex = localIndex;

    if (!IsAlive(startParticle))
        return false;

    float ribbonId = startParticle.Tangent.w;

    for (uint step = 1u; step < particleCount; ++step)
    {
        uint candidateLocalIndex = (localIndex + step) % particleCount;
        if (candidateLocalIndex == nextCursor)
            break;

        ParticleState candidate = particles[particleOffset + candidateLocalIndex];
        if (IsAlive(candidate) && SameRibbon(candidate.Tangent.w, ribbonId))
        {
            endParticle = candidate;
            endLocalIndex = candidateLocalIndex;
            return true;
        }
    }

    return false;
}

bool TryFindAdjacentParticle(
    uint localIndex,
    Emitter emitter,
    float ribbonId,
    bool forward,
    out ParticleState adjacentParticle
)
{
    uint particleOffset = (uint)emitter.MetaA.x;
    uint particleCount = (uint)emitter.MetaA.y;
    uint nextCursor = (uint)emitter.MetaC.w;
    uint newestLocalIndex = (nextCursor + particleCount - 1u) % particleCount;

    adjacentParticle = particles[particleOffset + localIndex];

    for (uint step = 1u; step < particleCount; ++step)
    {
        uint candidateLocalIndex = forward
            ? (localIndex + step) % particleCount
            : (localIndex + particleCount - step) % particleCount;

        if ((forward && candidateLocalIndex == nextCursor) ||
            (!forward && candidateLocalIndex == newestLocalIndex))
            break;

        ParticleState candidate = particles[particleOffset + candidateLocalIndex];
        if (IsAlive(candidate) && SameRibbon(candidate.Tangent.w, ribbonId))
        {
            adjacentParticle = candidate;
            return true;
        }
    }

    return false;
}

float3 BuildCameraFacingSide(uint localIndex, Emitter emitter, ParticleState particle)
{
    ParticleState previousParticle;
    ParticleState nextParticle;
    bool hasPrevious = TryFindAdjacentParticle(localIndex, emitter, particle.Tangent.w, false, previousParticle);
    bool hasNext = TryFindAdjacentParticle(localIndex, emitter, particle.Tangent.w, true, nextParticle);

    float3 tangent = particle.Tangent.xyz;
    if (hasPrevious && hasNext)
        tangent = nextParticle.PositionSize.xyz - previousParticle.PositionSize.xyz;
    else if (hasNext)
        tangent = nextParticle.PositionSize.xyz - particle.PositionSize.xyz;
    else if (hasPrevious)
        tangent = particle.PositionSize.xyz - previousParticle.PositionSize.xyz;

    tangent = SafeNormalize(tangent, float3(1.0, 0.0, 0.0));

    float3 viewDirection = SafeNormalize(CameraPosition - particle.PositionSize.xyz, float3(0.0, 0.0, 1.0));
    float3 side = cross(viewDirection, tangent);

    float3 helper = abs(tangent.y) < 0.99 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 fallbackSide = SafeNormalize(cross(helper, tangent), float3(1.0, 0.0, 0.0));
    side = SafeNormalize(side, fallbackSide);

    if (hasPrevious)
    {
        float3 previousDirection = SafeNormalize(
            particle.PositionSize.xyz - previousParticle.PositionSize.xyz,
            tangent
        );
        float3 previousSide = SafeNormalize(cross(viewDirection, previousDirection), side);
        if (dot(side, previousSide) < 0.0)
            side = -side;
    }
    else if (hasNext)
    {
        float3 nextDirection = SafeNormalize(
            nextParticle.PositionSize.xyz - particle.PositionSize.xyz,
            tangent
        );
        float3 nextSide = SafeNormalize(cross(viewDirection, nextDirection), side);
        if (dot(side, nextSide) < 0.0)
            side = -side;
    }

    return side;
}

VSOutput EmptyVertex()
{
    VSOutput output;
    output.ClipPos = float4(0.0, 0.0, 0.0, 1.0);
    output.Color = float4(0.0, 0.0, 0.0, 0.0);
    output.UV = float2(0.5, 0.0);
    return output;
}

VSOutput main(uint vertexId : SV_VertexID)
{
    Emitter emitter = emitters[pc.EmitterIndex];
    uint particleCount = (uint)emitter.MetaA.y;
    if (particleCount == 0)
        return EmptyVertex();

    uint segmentIndex = vertexId / 6u;
    uint vertexInSegment = vertexId % 6u;
    if (segmentIndex >= particleCount)
        return EmptyVertex();

    ParticleState startParticle;
    ParticleState endParticle;
    uint endLocalIndex;
    if (!TryBuildSegment(segmentIndex, emitter, startParticle, endParticle, endLocalIndex))
        return EmptyVertex();

    float3 p0 = startParticle.PositionSize.xyz;
    float3 p1 = endParticle.PositionSize.xyz;
    float width0 = max(startParticle.PositionSize.w * RIBBON_WORLD_SIZE_SCALE, 0.0001);
    float width1 = max(endParticle.PositionSize.w * RIBBON_WORLD_SIZE_SCALE, 0.0001);

    bool useEndParticle = vertexInSegment == 1u || vertexInSegment == 2u || vertexInSegment == 4u;
    bool usePositiveSide = vertexInSegment == 2u || vertexInSegment == 4u || vertexInSegment == 5u;

    float3 center = useEndParticle ? p1 : p0;
    float width = useEndParticle ? width1 : width0;
    float3 side = useEndParticle
        ? BuildCameraFacingSide(endLocalIndex, emitter, endParticle)
        : BuildCameraFacingSide(segmentIndex, emitter, startParticle);
    float sideSign = usePositiveSide ? 1.0 : -1.0;

    VSOutput output;
    output.ClipPos = mul(ViewProj, float4(center + side * (width * 0.5 * sideSign), 1.0));
    output.Color = useEndParticle ? endParticle.Color : startParticle.Color;
    output.UV = float2(usePositiveSide ? 1.0 : 0.0, useEndParticle ? 1.0 : 0.0);
    return output;
}
