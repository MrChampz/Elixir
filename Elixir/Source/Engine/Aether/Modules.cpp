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

    SetVelocityCone& SetVelocityCone::BindParameters(std::string angleParam,
        std::string minSpeedParam,
        std::string maxSpeedParam)
    {
        m_AngleParamName = std::move(angleParam);
        m_MinSpeedParamName = std::move(minSpeedParam);
        m_MaxSpeedParamName = std::move(maxSpeedParam);
        return *this;
    }

    /* SetLifetime */

    SetLifetime::SetLifetime(const float minSeconds, const float maxSeconds)
        : m_MinSeconds(minSeconds), m_MaxSeconds(maxSeconds) {}

    SetLifetime& SetLifetime::BindParameters(
        std::string minSecondsParam,
        std::string maxSecondsParam
    )
    {
        m_MinSecondsParamName = std::move(minSecondsParam);
        m_MaxSecondsParamName = std::move(maxSecondsParam);
        return *this;
    }

    /* SetSize */

    SetSize::SetSize(const float minSize, const float maxSize)
        : m_MinSize(minSize), m_MaxSize(maxSize) {}

    SetSize& SetSize::BindParameters(std::string minSizeParam, std::string maxSizeParam)
    {
        m_MinSizeParamName = std::move(minSizeParam);
        m_MaxSizeParamName = std::move(maxSizeParam);
        return *this;
    }

    /* SetColor */

    SetColor::SetColor(const glm::vec4 color) : m_Color(color) {}

    SetColor& SetColor::BindParameter(std::string paramName)
    {
        m_ParamName = std::move(paramName);
        return *this;
    }

    /* SetRotation */

    SetRotation::SetRotation(const float minRotation, const float maxRotation)
        : m_MinRotation(minRotation), m_MaxRotation(maxRotation) {}

    SetRotation& SetRotation::BindParameters(
        std::string minRotationParam,
        std::string maxRotationParam
    )
    {
        m_MinRotationParamName = std::move(minRotationParam);
        m_MaxRotationParamName = std::move(maxRotationParam);
        return *this;
    }

    /* SetScale */

    SetScale::SetScale(const float minScale, const float maxScale)
        : m_MinScale(minScale), m_MaxScale(maxScale) {}

    SetScale& SetScale::BindParameters(std::string minScaleParam, std::string maxScaleParam)
    {
        m_MinScaleParamName = std::move(minScaleParam);
        m_MaxScaleParamName = std::move(maxScaleParam);
        return *this;
    }

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

    /* SetRibbonId */

    SetRibbonId::SetRibbonId(const uint32_t ribbonId) : m_RibbonId(ribbonId) {}

    /* SetRibbonIdFromSpawnOrder */

    SetRibbonIdFromSpawnOrder::SetRibbonIdFromSpawnOrder(
        const uint32_t ribbonCount,
        const uint32_t firstRibbonId
    ) : m_RibbonCount(std::max(1u, ribbonCount)),
        m_FirstRibbonId(firstRibbonId) {}

    /* ApplyGravity */

    ApplyGravity::ApplyGravity(const glm::vec3 gravity) : m_Gravity(gravity) {}

    ApplyGravity& ApplyGravity::BindParameter(std::string paramName)
    {
        m_ParamName = std::move(paramName);
        return *this;
    }

    /* ApplyLinearDrag */

    ApplyLinearDrag::ApplyLinearDrag(const float dragPerSecond)
        : m_DragPerSecond(dragPerSecond) {}

    ApplyLinearDrag& ApplyLinearDrag::BindParameter(std::string paramName)
    {
        m_ParamName = std::move(paramName);
        return *this;
    }

    /* ApplyAngularVelocity */

    ApplyAngularVelocity::ApplyAngularVelocity(const float radiansPerSecond)
        : m_RadiansPerSecond(radiansPerSecond) {}

    ApplyAngularVelocity& ApplyAngularVelocity::BindParameter(std::string paramName)
    {
        m_ParamName = std::move(paramName);
        return *this;
    }

    ApplyAngularVelocity& ApplyAngularVelocity::BindInput(EDynamicInput input)
    {
        m_Input = input;
        return *this;
    }

    /* ColorOverLife */

    ColorOverLife::ColorOverLife(const glm::vec4 startColor, const glm::vec4 endColor)
        : m_StartColor(startColor), m_EndColor(endColor) {}

    ColorOverLife& ColorOverLife::BindParameters(
        std::string startColorParam,
        std::string endColorParam
    )
    {
        m_StartColorParamName = std::move(startColorParam);
        m_EndColorParamName = std::move(endColorParam);
        return *this;
    }

    /* SizeOverLife */

    SizeOverLife::SizeOverLife(const float startSize, const float endSize)
        : m_StartSize(startSize), m_EndSize(endSize) {}

    SizeOverLife& SizeOverLife::BindParameters(
        std::string startSizeParam,
        std::string endSizeParam
    )
    {
        m_StartSizeParamName = std::move(startSizeParam);
        m_EndSizeParamName = std::move(endSizeParam);
        return *this;
    }

    /* ScaleOverLife */

    ScaleOverLife::ScaleOverLife(const float startScale, const float endScale)
        : m_StartScale(startScale), m_EndScale(endScale) {}

    ScaleOverLife& ScaleOverLife::BindParameters(
        std::string startScaleParam,
        std::string endScaleParam
    )
    {
        m_StartScaleParamName = std::move(startScaleParam);
        m_EndScaleParamName = std::move(endScaleParam);
        return *this;
    }

    ScaleOverLife& ScaleOverLife::BindCurve(std::string curveName, EDynamicInput input)
    {
        m_CurveName = std::move(curveName);
        m_CurveInput = input;
        return *this;
    }

    /* KillOutsideBounds */

    KillOutsideBounds::KillOutsideBounds(const glm::vec3 min, const glm::vec3 max)
        : m_Min(min), m_Max(max) {}
}