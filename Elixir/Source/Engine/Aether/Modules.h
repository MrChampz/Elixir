#pragma once
#include "Particle.h"

namespace Elixir::Aether
{
    struct SParticle;
    class ParameterStore;

    enum class EParticleOp : uint32_t
    {
        SetLiteral = 0,
        RandomRange,
        SampleDisk,
        SampleCone,
        SampleBox,
        AddWithDelta,
        Dampen,
        LerpOverLife,
        KillOutsideBounds,
        AddFromAttribute,
        SetPositionOnCircle,
        SetPositionCircularPath,
        SetRibbonIdFromSpawnOrder,
        SampleCurve,
        SampleColorCurve,
        Add,
        Mul,
        Clamp
    };

    struct SGPUParticleOp
    {
        EParticleOp Type = EParticleOp::SetLiteral;
        EParticleAttribute Target = EParticleAttribute::None;
        uint32_t Parameter0Index = UINT32_MAX;
        uint32_t Parameter1Index = UINT32_MAX;
        glm::vec4 Data0{};
        glm::vec4 Data1{};
        glm::vec4 Data2{};
    };

    enum class EDynamicInput : uint32_t
    {
        None = 0,
        DeltaTime,
        NormalizedAge,
        EmitterTime,
        Random,
        ParticleSeed
    };

    class ParticleSpawnModule
    {
    public:
        virtual ~ParticleSpawnModule() = default;
    };

    class ParticleUpdateModule
    {
    public:
        virtual ~ParticleUpdateModule() = default;
    };

    class ELIXIR_API SetPositionDisk final : public ParticleSpawnModule
    {
    public:
        explicit SetPositionDisk(glm::vec3 center, float radius, glm::vec3 normal = { 0, 1, 0 });

        glm::vec3 GetCenter() const { return m_Center; }
        glm::vec3 GetNormal() const { return m_Normal; }
        float GetRadius() const { return m_Radius; }

    private:
        glm::vec3 m_Center;
        glm::vec3 m_Normal;
        float m_Radius;
    };

    class ELIXIR_API SetPositionBox final : public ParticleSpawnModule
    {
    public:
        explicit SetPositionBox(glm::vec3 minBounds, glm::vec3 maxBounds);

        glm::vec3 GetMinBounds() const { return m_MinBounds; }
        glm::vec3 GetMaxBounds() const { return m_MaxBounds; }

    private:
        glm::vec3 m_MinBounds;
        glm::vec3 m_MaxBounds;
    };

    class ELIXIR_API SetVelocityCone final : public ParticleSpawnModule
    {
    public:
        explicit SetVelocityCone(glm::vec3 direction, float angle, float minSpeed, float maxSpeed);

        SetVelocityCone& BindParameters(std::string angleParam, std::string minSpeedParam, std::string maxSpeedParam);

        glm::vec3 GetDirection() const { return m_Direction; }
        float GetAngle() const { return m_Angle; }
        float GetMinSpeed() const { return m_MinSpeed; }
        float GetMaxSpeed() const { return m_MaxSpeed; }

        const std::string& GetAngleParamName() const { return m_AngleParamName; }
        const std::string& GetMinSpeedParamName() const { return m_MinSpeedParamName; }
        const std::string& GetMaxSpeedParamName() const { return m_MaxSpeedParamName; }

    private:
        glm::vec3 m_Direction;
        float m_Angle;      // radians
        float m_MinSpeed;
        float m_MaxSpeed;
        std::string m_AngleParamName;
        std::string m_MinSpeedParamName;
        std::string m_MaxSpeedParamName;
    };

    class ELIXIR_API SetLifetime final : public ParticleSpawnModule
    {
    public:
        explicit SetLifetime(float minSeconds, float maxSeconds);

        SetLifetime& BindParameters(std::string minSecondsParam, std::string maxSecondsParam);

        float GetMinSeconds() const { return m_MinSeconds; }
        float GetMaxSeconds() const { return m_MaxSeconds; }
        const std::string& GetMinSecondsParamName() const { return m_MinSecondsParamName; }
        const std::string& GetMaxSecondsParamName() const { return m_MaxSecondsParamName; }

    private:
        float m_MinSeconds;
        float m_MaxSeconds;
        std::string m_MinSecondsParamName;
        std::string m_MaxSecondsParamName;
    };

    class ELIXIR_API SetSize final : public ParticleSpawnModule
    {
    public:
        explicit SetSize(float minSize, float maxSize);

        SetSize& BindParameters(std::string minSizeParam, std::string maxSizeParam);

        float GetMinSize() const { return m_MinSize; }
        float GetMaxSize() const { return m_MaxSize; }
        const std::string& GetMinSizeParamName() const { return m_MinSizeParamName; }
        const std::string& GetMaxSizeParamName() const { return m_MaxSizeParamName; }

    private:
        float m_MinSize;
        float m_MaxSize;
        std::string m_MinSizeParamName;
        std::string m_MaxSizeParamName;
    };

    class ELIXIR_API SetColor final : public ParticleSpawnModule
    {
    public:
        explicit SetColor(glm::vec4 color);

        SetColor& BindParameter(std::string paramName);

        const glm::vec4& GetColor() const { return m_Color; }
        const std::string& GetParamName() const { return m_ParamName; }

    private:
        glm::vec4 m_Color;
        std::string m_ParamName;
    };

    class ELIXIR_API SetRotation final : public ParticleSpawnModule
    {
    public:
        explicit SetRotation(float minRotation, float maxRotation);

        SetRotation& BindParameters(std::string minRotationParam, std::string maxRotationParam);

        float GetMinRotation() const { return m_MinRotation; }
        float GetMaxRotation() const { return m_MaxRotation; }
        const std::string& GetMinRotationParamName() const { return m_MinRotationParamName; }
        const std::string& GetMaxRotationParamName() const { return m_MaxRotationParamName; }

    private:
        float m_MinRotation;
        float m_MaxRotation;
        std::string m_MinRotationParamName;
        std::string m_MaxRotationParamName;

    };

    class ELIXIR_API SetScale final : public ParticleSpawnModule
    {
    public:
        explicit SetScale(float minScale, float maxScale);

        SetScale& BindParameters(std::string minScaleParam, std::string maxScaleParam);

        float GetMinScale() const { return m_MinScale; }
        float GetMaxScale() const { return m_MaxScale; }
        const std::string& GetMinScaleParamName() const { return m_MinScaleParamName; }
        const std::string& GetMaxScaleParamName() const { return m_MaxScaleParamName; }

    private:
        float m_MinScale;
        float m_MaxScale;
        std::string m_MinScaleParamName;
        std::string m_MaxScaleParamName;
    };

    class ELIXIR_API SetPositionOnCircle final : public ParticleSpawnModule
    {
    public:
        explicit SetPositionOnCircle(glm::vec3 center, float radius, float angularSpeed, float startAngle = 0.0f);

        glm::vec3 GetCenter() const { return m_Center; }
        float GetRadius() const { return m_Radius; }
        float GetAngularSpeed() const { return m_AngularSpeed; }
        float GetStartAngle() const { return m_StartAngle; }

    private:
        glm::vec3 m_Center;
        float m_Radius;
        float m_AngularSpeed;
        float m_StartAngle;
    };

    class ELIXIR_API SetPositionCircularPath final : public ParticleSpawnModule
    {
    public:
        explicit SetPositionCircularPath(
            glm::vec3 baseOffset,
            glm::vec3 primaryAmplitude,
            glm::vec3 secondaryAmplitude,
            float timeScale
        );

        glm::vec3 GetBaseOffset() const { return m_BaseOffset; }
        glm::vec3 GetPrimaryAmplitude() const { return m_PrimaryAmplitude; }
        glm::vec3 GetSecondaryAmplitude() const { return m_SecondaryAmplitude; }
        float GetTimeScale() const { return m_TimeScale; }

    private:
        glm::vec3 m_BaseOffset;
        glm::vec3 m_PrimaryAmplitude;
        glm::vec3 m_SecondaryAmplitude;
        float m_TimeScale;
    };

    class ELIXIR_API SetRibbonId final : public ParticleSpawnModule
    {
      public:
        explicit SetRibbonId(uint32_t ribbonId);

        uint32_t GetRibbonId() const { return m_RibbonId; }

      private:
        uint32_t m_RibbonId;
    };

    class ELIXIR_API SetRibbonIdFromSpawnOrder final : public ParticleSpawnModule
    {
      public:
        explicit SetRibbonIdFromSpawnOrder(uint32_t ribbonCount, uint32_t firstRibbonId = 0);

        uint32_t GetRibbonCount() const { return m_RibbonCount; }
        uint32_t GetFirstRibbonId() const { return m_FirstRibbonId; }

      private:
        uint32_t m_RibbonCount;
        uint32_t m_FirstRibbonId;
    };

    class ELIXIR_API ApplyGravity final : public ParticleUpdateModule
    {
      public:
        explicit ApplyGravity(glm::vec3 gravity);

        ApplyGravity& BindParameter(std::string paramName);

        const glm::vec3& GetGravity() const { return m_Gravity; }
        const std::string& GetParamName() const { return m_ParamName; }

      private:
        glm::vec3 m_Gravity;
        std::string m_ParamName;
    };

    class ELIXIR_API ApplyLinearDrag final : public ParticleUpdateModule
    {
      public:
        explicit ApplyLinearDrag(float dragPerSecond);

        ApplyLinearDrag& BindParameter(std::string paramName);

        float GetDragPerSecond() const { return m_DragPerSecond; }
        const std::string& GetParamName() const { return m_ParamName; }

      private:
        float m_DragPerSecond;
        std::string m_ParamName;
    };

    class ELIXIR_API ApplyAngularVelocity final : public ParticleUpdateModule
    {
    public:
        explicit ApplyAngularVelocity(float radiansPerSecond);

        ApplyAngularVelocity& BindParameter(std::string paramName);
        ApplyAngularVelocity& BindInput(EDynamicInput input);

        float GetRadiansPerSecond() const { return m_RadiansPerSecond; }
        const std::string& GetParamName() const { return m_ParamName; }
        EDynamicInput GetInput() const { return m_Input; }

    private:
        float m_RadiansPerSecond;
        std::string m_ParamName;
        EDynamicInput m_Input = EDynamicInput::None;
    };

    class ELIXIR_API ColorOverLife final : public ParticleUpdateModule
    {
      public:
        explicit ColorOverLife(glm::vec4 startColor, glm::vec4 endColor);

        ColorOverLife& BindParameters(std::string startColorParam, std::string endColorParam);
        ColorOverLife& BindCurve(std::string name, EDynamicInput input = EDynamicInput::NormalizedAge);

        const glm::vec4& GetStartColor() const { return m_StartColor; }
        const glm::vec4& GetEndColor() const { return m_EndColor; }
        const std::string& GetStartColorParamName() const { return m_StartColorParamName; }
        const std::string& GetEndColorParamName() const { return m_EndColorParamName; }
        const std::string& GetCurveName() const { return m_CurveName; }
        EDynamicInput GetCurveInput() const { return m_CurveInput; }

      private:
        glm::vec4 m_StartColor;
        glm::vec4 m_EndColor;
        std::string m_StartColorParamName;
        std::string m_EndColorParamName;
        std::string m_CurveName;
        EDynamicInput m_CurveInput = EDynamicInput::None;
    };

    class ELIXIR_API SizeOverLife final : public ParticleUpdateModule
    {
    public:
        SizeOverLife(float startSize, float endSize);

        SizeOverLife& BindParameters(std::string startSizeParam, std::string endSizeParam);

        float GetStartSize() const { return m_StartSize; }
        float GetEndSize() const { return m_EndSize; }
        const std::string& GetStartSizeParamName() const { return m_StartSizeParamName; }
        const std::string& GetEndSizeParamName() const { return m_EndSizeParamName; }

    private:
        float m_StartSize;
        float m_EndSize;
        std::string m_StartSizeParamName;
        std::string m_EndSizeParamName;
    };

    class ELIXIR_API ScaleOverLife final : public ParticleUpdateModule
    {
    public:
        ScaleOverLife(float startScale, float endScale);

        ScaleOverLife& BindParameters(std::string startScaleParam, std::string endScaleParam);
        ScaleOverLife& BindCurve(std::string curveName, EDynamicInput input = EDynamicInput::NormalizedAge);

        float GetStartScale() const { return m_StartScale; }
        float GetEndScale() const { return m_EndScale; }
        const std::string& GetStartScaleParamName() const { return m_StartScaleParamName; }
        const std::string& GetEndScaleParamName() const { return m_EndScaleParamName; }
        const std::string& GetCurveName() const { return m_CurveName; }
        EDynamicInput GetCurveInput() const { return m_CurveInput; }

    private:
        float m_StartScale;
        float m_EndScale;
        std::string m_StartScaleParamName;
        std::string m_EndScaleParamName;
        std::string m_CurveName;
        EDynamicInput m_CurveInput = EDynamicInput::None;
    };

    class ELIXIR_API KillOutsideBounds final : public ParticleUpdateModule
    {
      public:
        explicit KillOutsideBounds(glm::vec3 min, glm::vec3 max);

        glm::vec3 GetMin() const { return m_Min; }
        glm::vec3 GetMax() const { return m_Max; }

      private:
        glm::vec3 m_Min;
        glm::vec3 m_Max;
    };
}