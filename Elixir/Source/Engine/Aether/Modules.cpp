#include "epch.h"
#include "Modules.h"

#include "Particle.h"
#include "System.h"

#include <numbers>

namespace Elixir::Aether
{
    float randomRange(std::mt19937& rng, const float min, const float max)
    {
        std::uniform_real_distribution distribution(min, max);
        return distribution(rng);
    }

    glm::vec2 fromAngle(const float radians) {
        return { glm::cos(radians), glm::sin(radians) };
    }

    /* SetPositionCircle */

    SetPositionCircle::SetPositionCircle(const glm::vec2 center, const float radius)
        : m_Center(center), m_Radius(radius) {}

    void SetPositionCircle::Apply(
        SParticle& particle,
        SSpawnContext& context,
        const ParameterStore& params
    ) const
    {
        const float angle = randomRange(context.RNG, 0.0f, 2.0f * std::numbers::pi_v<float>);
        const float distance = std::sqrt(randomRange(context.RNG, 0.0f, 1.0f)) * m_Radius;
        particle.Position = m_Center + (fromAngle(angle) * distance);
    }

    /* SetPositionOnCircle */

    SetPositionOnCircle::SetPositionOnCircle(
        const glm::vec2 center,
        const float radius,
        const float angularSpeed,
        const float startAngle
    ) : m_Center(center),
        m_Radius(radius),
        m_AngularSpeed(angularSpeed),
        m_StartAngle(startAngle) {}

    void SetPositionOnCircle::Apply(
        SParticle& particle,
        SSpawnContext& context,
        const ParameterStore& params
    ) const
    {
        // CPU stub: GPU spawn shader uses elapsed time; CPU path drops the sample at startAngle.
        particle.Position = m_Center + fromAngle(m_StartAngle) * m_Radius;
    }

    /* SetPositionBezierLoop */

    SetPositionBezierLoop::SetPositionBezierLoop(
        const glm::vec2 start,
        const glm::vec2 controlA,
        const glm::vec2 controlB,
        const glm::vec2 end,
        const float duration,
        const float startOffset,
        const float segmentStart,
        const float segmentLength
    ) : m_Start(start),
        m_ControlA(controlA),
        m_ControlB(controlB),
        m_End(end),
        m_Duration(duration),
        m_StartOffset(startOffset),
        m_SegmentStart(segmentStart),
        m_SegmentLength(segmentLength) {}

    void SetPositionBezierLoop::Apply(
        SParticle& particle,
        SSpawnContext& context,
        const ParameterStore& params
    ) const
    {
        // CPU stub: GPU spawn shader advances along the loop using elapsed time.
        particle.Position = m_Start;
    }

    /* SetVelocityCone */

    SetVelocityCone::SetVelocityCone(
        const float minAngle,
        const float maxAngle,
        const float minSpeed,
        const float maxSpeed
    ) : m_MinAngle(minAngle),
        m_MaxAngle(maxAngle),
        m_MinSpeed(minSpeed),
        m_MaxSpeed(maxSpeed) {}

    void SetVelocityCone::Apply(
        SParticle& particle,
        SSpawnContext& context,
        const ParameterStore& params
    ) const
    {
        const float angle = randomRange(context.RNG, m_MinAngle, m_MaxAngle);
        const float speed = randomRange(context.RNG, m_MinSpeed, m_MaxSpeed);
        particle.Velocity = fromAngle(angle) * speed;
    }

    /* SetLifetime */

    SetLifetime::SetLifetime(const float minSeconds, const float maxSeconds)
        : m_MinSeconds(minSeconds), m_MaxSeconds(maxSeconds) {}

    void SetLifetime::Apply(
        SParticle& particle,
        SSpawnContext& context,
        const ParameterStore& params
    ) const
    {
        particle.Lifetime = randomRange(context.RNG, m_MinSeconds, m_MaxSeconds);
    }

    /* SetSize */

    SetSize::SetSize(const float minSize, const float maxSize)
        : m_MinSize(minSize), m_MaxSize(maxSize) {}

    void SetSize::Apply(
        SParticle& particle,
        SSpawnContext& context,
        const ParameterStore& params
    ) const
    {
        particle.Size = randomRange(context.RNG, m_MinSize, m_MaxSize);
    }

    /* SetColor */

    SetColor::SetColor(const glm::vec4 color) : m_Color(color) {}

    void SetColor::Apply(
        SParticle& particle,
        SSpawnContext& context,
        const ParameterStore& params
    ) const
    {
        particle.Color = m_Color;
    }

    /* ApplyGravity */

    ApplyGravity::ApplyGravity(const glm::vec2 gravity) : m_Gravity(gravity) {}

    void ApplyGravity::Apply(
        SParticle& particle,
        SUpdateContext& context,
        const ParameterStore& params
    ) const
    {
        const float gravityScale = params.GetFloat("GravityScale", 1.0f);
        particle.Velocity += m_Gravity * gravityScale * context.deltaTime;
    }

    /* ApplyLinearDrag */

    ApplyLinearDrag::ApplyLinearDrag(const float dragPerSecond)
        : m_DragPerSecond(dragPerSecond) {}

    void ApplyLinearDrag::Apply(
        SParticle& particle,
        SUpdateContext& context,
        const ParameterStore& params
    ) const
    {
        const float drag = std::max(0.0f, 1.0f - (m_DragPerSecond * context.deltaTime));
        particle.Velocity *= drag;
    }

    /* IntegrateVelocity */

    void IntegrateVelocity::Apply(
        SParticle& particle,
        SUpdateContext& context,
        const ParameterStore& params
    ) const
    {
        particle.Position += particle.Velocity * context.deltaTime;
    }

    /* ColorOverLife */

    ColorOverLife::ColorOverLife(const glm::vec4 startColor, const glm::vec4 endColor)
        : m_StartColor(startColor), m_EndColor(endColor) {}

    void ColorOverLife::Apply(
        SParticle& particle,
        SUpdateContext& context,
        const ParameterStore& params
    ) const
    {
        const float normalizedAge = particle.Lifetime > 0.0f ? (particle.Age / particle.Lifetime) : 1.0f;
        particle.Color = glm::mix(m_StartColor, m_EndColor, normalizedAge);
    }

    /* SizeOverLife */

    SizeOverLife::SizeOverLife(const float startSize, const float endSize)
        : m_StartSize(startSize), m_EndSize(endSize) {}

    void SizeOverLife::Apply(
        SParticle& particle,
        SUpdateContext& context,
        const ParameterStore& params
    ) const
    {
        const float normalizedAge = particle.Lifetime > 0.0f ? (particle.Age / particle.Lifetime) : 1.0f;
        particle.Size = m_StartSize + ((m_EndSize - m_StartSize) * glm::clamp(normalizedAge, 0.0f, 1.0f));
    }

    /* KillOutsideBounds */

    KillOutsideBounds::KillOutsideBounds(const glm::vec2 min, const glm::vec2 max)
        : m_Min(min), m_Max(max) {}

    void KillOutsideBounds::Apply(
        SParticle& particle,
        SUpdateContext& context,
        const ParameterStore& params
    ) const
    {
        if (particle.Position.x < m_Min.x || particle.Position.x > m_Max.x ||
            particle.Position.y < m_Min.y || particle.Position.y > m_Max.y)
        {
            particle.Alive = false;
        }
    }
}
