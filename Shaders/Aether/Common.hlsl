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

float LinkOrder(ParticleState particle)
{
    return particle.Metadata.y;
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

    startParticle = particles[particleOffset + localIndex];
    endParticle = startParticle;
    endLocalIndex = localIndex;

    if (!IsAlive(startParticle))
        return false;

    float ribbonId = startParticle.Tangent.w;
    float startLinkOrder = LinkOrder(startParticle);
    float bestLinkDelta = 3.402823466e+38;
    bool found = false;

    for (uint candidateLocalIndex = 0u; candidateLocalIndex < particleCount; ++candidateLocalIndex)
    {
        if (candidateLocalIndex == localIndex)
            continue;

        ParticleState candidate = particles[particleOffset + candidateLocalIndex];
        float candidateLinkDelta = LinkOrder(candidate) - startLinkOrder;

        if (!IsAlive(candidate) ||
            !SameRibbon(candidate.Tangent.w, ribbonId) ||
            candidateLinkDelta <= 0.0 ||
            candidateLinkDelta >= bestLinkDelta)
            continue;

        endParticle = candidate;
        endLocalIndex = candidateLocalIndex;
        bestLinkDelta = candidateLinkDelta;
        found = true;
    }

    return found;
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