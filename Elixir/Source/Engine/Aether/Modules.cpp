#include "epch.h"
#include "Modules.h"

#include "Particle.h"

namespace Elixir::Aether
{
    /* SetPositionDisk */

    SetPositionDisk::SetPositionDisk(const glm::vec3 center, const float radius, const glm::vec3 normal)
        : m_Center(center), m_Normal(normal), m_Radius(radius) {}

    /* SetVelocityCone */

    SetVelocityCone::SetVelocityCone(
        const glm::vec3 direction,
        const float angle,
        const float minSpeed,
        const float maxSpeed
    ) : m_Direction(direction),
        m_Angle(angle),
        m_MinSpeed(minSpeed),
        m_MaxSpeed(maxSpeed) {}

    /* SetLifetime */

    SetLifetime::SetLifetime(const float minSeconds, const float maxSeconds)
        : m_MinSeconds(minSeconds), m_MaxSeconds(maxSeconds) {}

    /* SetSize */

    SetSize::SetSize(const float minSize, const float maxSize)
        : m_MinSize(minSize), m_MaxSize(maxSize) {}

    /* SetColor */

    SetColor::SetColor(const glm::vec4 color) : m_Color(color) {}

    /* SetPositionOnCircle */

    SetPositionOnCircle::SetPositionOnCircle(
        const glm::vec3 center,
        const float radius,
        const float angularSpeed,
        const float startAngle
    ) : m_Center(center),
        m_Radius(radius),
        m_AngularSpeed(angularSpeed),
        m_StartAngle(startAngle) {}

    /* SetPositionCircularPath */

    SetPositionCircularPath::SetPositionCircularPath(
        const glm::vec3 baseOffset,
        const glm::vec3 primaryAmplitude,
        const glm::vec3 secondaryAmplitude,
        const float timeScale
    ) : m_BaseOffset(baseOffset),
        m_PrimaryAmplitude(primaryAmplitude),
        m_SecondaryAmplitude(secondaryAmplitude),
        m_TimeScale(timeScale) {}

    /* ApplyGravity */

    ApplyGravity::ApplyGravity(const glm::vec3 gravity) : m_Gravity(gravity) {}

    /* ApplyLinearDrag */

    ApplyLinearDrag::ApplyLinearDrag(const float dragPerSecond)
        : m_DragPerSecond(dragPerSecond) {}

    /* ColorOverLife */

    ColorOverLife::ColorOverLife(const glm::vec4 startColor, const glm::vec4 endColor)
        : m_StartColor(startColor), m_EndColor(endColor) {}

    /* SizeOverLife */

    SizeOverLife::SizeOverLife(const float startSize, const float endSize)
        : m_StartSize(startSize), m_EndSize(endSize) {}

    /* KillOutsideBounds */

    KillOutsideBounds::KillOutsideBounds(const glm::vec3 min, const glm::vec3 max)
        : m_Min(min), m_Max(max) {}
}