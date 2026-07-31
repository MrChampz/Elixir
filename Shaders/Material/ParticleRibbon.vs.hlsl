// Template for EMaterialUsage::ParticleRibbon.

struct ParticleState
{
    float4 PositionSize;    // xyz = position, w = size
    float4 VelocityAge;     // xyz = velocity, w = age
    float4 Transform;       // x = rotation, y = scale
    float4 TangentRibbonId; // xyz = tangent, w = ribbon id
    float4 Color;
    float4 Metadata;        // x = emitter index, y = ribbon link order, z = lifetime, w = alive
};


[[vk::binding(1, 0)]]
StructuredBuffer<ParticleState> particles;

struct Emitter
{
    float4 MetaA; // x = offset in particle buffer, y = max particles, z = module offset(spawn), w = module count(spawn)
    float4 MetaB; // x = module offset(update), y = module count(update), z = buffer cursor, w = spawn count
    float4 MetaC; // x = render mode, y = spawn rate seconds, z = gravity scale, w = next buffer cursor
    float4 MetaD; // x = emission index
};


[[vk::binding(2, 0)]]
StructuredBuffer<Emitter> emitters;

[[vk::binding(0, 0)]]
cbuffer cbFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float _Padding;
};

struct PushConstants
{
    float4x4 WorldTransform;
    uint EmitterIndex;
    uint ParticleBaseOffset;
    uint MaterialIndex;
};

[[vk::push_constant]]
PushConstants pc;

struct VSOutput
{
    float4 ClipPos              : SV_POSITION;
    float4 Color                : COLOR0;
    float2 TexCoord             : TEXCOORD0;
    nointerpolation float Valid : TEXCOORD1;
};

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

uint LinkOrder(ParticleState particle)
{
    return asuint(particle.Metadata.y);
}

// Maximum number of interleaved ribbons per emitter. Particles belonging to the
// same ribbon are spaced exactly (ribbonCount) slots apart in the circular buffer
// because emissionIndex increments by 1 per spawn and ribbons are assigned round-robin.
// Searching only this many slots forward reduces TryBuildSegment from O(N) to O(W),
// turning the overall O(N²) vertex shader cost into O(W·N).
// Raise this value only if an emitter needs more than 32 simultaneous ribbons.
#define RIBBON_SEARCH_WINDOW 32

bool TryBuildSegment(
    uint localIndex,
    uint particleBaseOffset,
    Emitter emitter,
    out ParticleState startParticle,
    out ParticleState endParticle,
    out uint endLocalIndex
)
{
    uint particleOffset = particleBaseOffset + (uint)emitter.MetaA.x;
    uint particleCount  = (uint)emitter.MetaA.y;

    startParticle = particles[particleOffset + localIndex];
    endParticle   = startParticle;
    endLocalIndex = localIndex;

    if (!IsAlive(startParticle))
        return false;

    float ribbonId      = startParticle.TangentRibbonId.w;
    uint startLinkOrder = LinkOrder(startParticle);
    uint bestLinkDelta  = 0xFFFFFFFFu; // UINT_MAX
    bool  found         = false;

    // Ribbons are assigned round-robin at spawn time, so the successor particle
    // in a ribbon is always within RIBBON_SEARCH_WINDOW slots ahead in the
    // circular buffer. We search forward only, relying on the invariant that a
    // valid successor has a positive and minimal linkOrder delta.
    uint windowSize = min(RIBBON_SEARCH_WINDOW, particleCount - 1u);
    for (uint step = 1u; step <= windowSize; ++step)
    {
        uint candidateLocalIndex = (localIndex + step) % particleCount;
        ParticleState candidate  = particles[particleOffset + candidateLocalIndex];
        uint candidateLinkOrder  = LinkOrder(candidate);

        // Compare before subtracting: if candidateLinkOrder <= startLinkOrder the
        // subtraction would wrap around to a near-UINT_MAX value and falsely pass
        // the range check, connecting the ribbon start to stale/old-cycle particles.
        if (!IsAlive(candidate) ||
            !SameRibbon(candidate.TangentRibbonId.w, ribbonId) ||
            candidateLinkOrder <= startLinkOrder)
            continue;

        uint candidateLinkDelta = candidateLinkOrder - startLinkOrder;
        if (candidateLinkDelta < bestLinkDelta)
        {
            endParticle   = candidate;
            endLocalIndex = candidateLocalIndex;
            bestLinkDelta = candidateLinkDelta;
            found         = true;
        }
    }

    return found;
}

float3 BuildSegmentSide(
    float3 p0,
    float3 p1,
    ParticleState startParticle,
    ParticleState endParticle
)
{
    float3 tangentFallback = SafeNormalize(
        startParticle.TangentRibbonId.xyz + endParticle.TangentRibbonId.xyz,
        float3(1.0, 0.0, 0.0)
    );

    float3 segmentDirection = SafeNormalize(p1 - p0, tangentFallback);
    float3 segmentCenter = (p0 + p1) * 0.5;
    float3 viewDirection = SafeNormalize(CameraPos - segmentCenter, float3(0.0, 0.0, 1.0));
    float3 side = cross(viewDirection, segmentDirection);

    float3 helper = abs(segmentDirection.y) < 0.99 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 fallbackSide = SafeNormalize(cross(helper, segmentDirection), float3(1.0, 0.0, 0.0));

    return SafeNormalize(side, fallbackSide);
}

float3 BuildDirectionSide(float3 viewDirection, float3 direction, float3 referenceSide)
{
    float3 side = SafeNormalize(cross(viewDirection, direction), referenceSide);
    if (dot(side, referenceSide) < 0.0)
        side = -side;

    return side;
}

float3 BuildParticleSide(ParticleState particle, float3 referenceSide)
{
    float3 tangentFallback = SafeNormalize(particle.TangentRibbonId.xyz, float3(1.0, 0.0, 0.0));
    float3 viewDirection = SafeNormalize(CameraPos - particle.PositionSize.xyz, float3(0.0, 0.0, 1.0));
    return BuildDirectionSide(viewDirection, tangentFallback, referenceSide);
}

static const float RIBBON_WORLD_SIZE_SCALE = 0.01;

VSOutput EmptyVertex()
{
    VSOutput output;
    output.ClipPos = float4(0.0, 0.0, 0.0, 1.0);
    output.Color = float4(0.0, 0.0, 0.0, 0.0);
    output.TexCoord = float2(0.5, 0.0);
    output.Valid = 0.0;
    return output;
}

// TODO: TEMP approximation, replace by a better solution!
float MaxAxisScale(float4x4 transform)
{
    return max(
        length(transform[0].xyz),
        max(length(transform[1].xyz), length(transform[2].xyz))
    );
}

VSOutput main(uint vertexId : SV_VertexID)
{
    Emitter emitter = emitters[pc.EmitterIndex];
    uint particleCount = (uint)emitter.MetaA.y;
    if (particleCount == 0u)
        return EmptyVertex();

    uint segmentIndex = vertexId / 6u;
    uint vertexInSegment = vertexId % 6u;
    if (segmentIndex >= particleCount)
        return EmptyVertex();

    ParticleState startParticle;
    ParticleState endParticle;
    uint endLocalIndex;

    if (!TryBuildSegment(
        segmentIndex,
        pc.ParticleBaseOffset,
        emitter,
        startParticle,
        endParticle,
        endLocalIndex
    ))
        return EmptyVertex();

    float3x3 worldLinearTransform = (float3x3)pc.WorldTransform;

    startParticle.PositionSize.xyz =
        mul(pc.WorldTransform, float4(startParticle.PositionSize.xyz, 1.0f)).xyz;
    startParticle.VelocityAge.xyz =
        mul(worldLinearTransform, startParticle.VelocityAge.xyz);
    startParticle.TangentRibbonId.xyz =
        mul(worldLinearTransform, startParticle.TangentRibbonId.xyz);

    endParticle.PositionSize.xyz =
        mul(pc.WorldTransform, float4(endParticle.PositionSize.xyz, 1.0f)).xyz;
    endParticle.VelocityAge.xyz =
        mul(worldLinearTransform, endParticle.VelocityAge.xyz);
    endParticle.TangentRibbonId.xyz =
        mul(worldLinearTransform, endParticle.TangentRibbonId.xyz);

    float3 p0 = startParticle.PositionSize.xyz;
    float3 p1 = endParticle.PositionSize.xyz;
    float scale0 = max(startParticle.Transform.y, 0.0);
    float scale1 = max(endParticle.Transform.y, 0.0);
    float transformScale = MaxAxisScale(pc.WorldTransform);
    float width0 = max(startParticle.PositionSize.w * scale0 * transformScale * RIBBON_WORLD_SIZE_SCALE, 0.0001);
    float width1 = max(endParticle.PositionSize.w * scale1 * transformScale * RIBBON_WORLD_SIZE_SCALE, 0.0001);
    float3 segmentSide = BuildSegmentSide(p0, p1, startParticle, endParticle);
    float3 startSide = BuildParticleSide(startParticle, segmentSide);
    float3 endSide = BuildParticleSide(endParticle, segmentSide);

    bool useEndParticle = vertexInSegment == 1u || vertexInSegment == 2u || vertexInSegment == 4u;
    bool usePositiveSide = vertexInSegment == 2u || vertexInSegment == 4u || vertexInSegment == 5u;

    float3 center = useEndParticle ? p1 : p0;
    float width = useEndParticle ? width1 : width0;
    float3 side = useEndParticle ? endSide : startSide;
    float sideSign = usePositiveSide ? 1.0 : -1.0;

    VSOutput output;
    output.ClipPos = mul(ViewProj, float4(center + side * (width * 0.5 * sideSign), 1.0));
    output.Color = useEndParticle ? endParticle.Color : startParticle.Color;
    output.TexCoord = float2(usePositiveSide ? 1.0 : 0.0, useEndParticle ? 1.0 : 0.0);
    output.Valid = 1.0;

    return output;
}