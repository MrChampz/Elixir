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
    Emitter emitter,
    out ParticleState startParticle,
    out ParticleState endParticle,
    out uint endLocalIndex
)
{
    uint particleOffset = (uint)emitter.MetaA.x;
    uint particleCount  = (uint)emitter.MetaA.y;

    startParticle = particles[particleOffset + localIndex];
    endParticle   = startParticle;
    endLocalIndex = localIndex;

    if (!IsAlive(startParticle))
        return false;

    float ribbonId      = startParticle.Tangent.w;
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
            !SameRibbon(candidate.Tangent.w, ribbonId) ||
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
        startParticle.Tangent.xyz + endParticle.Tangent.xyz,
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
    float3 tangentFallback = SafeNormalize(particle.Tangent.xyz, float3(1.0, 0.0, 0.0));
    float3 viewDirection = SafeNormalize(CameraPos - particle.PositionSize.xyz, float3(0.0, 0.0, 1.0));
    return BuildDirectionSide(viewDirection, tangentFallback, referenceSide);
}