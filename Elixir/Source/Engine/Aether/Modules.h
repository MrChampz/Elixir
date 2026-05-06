#pragma once
#include "Particle.h"

namespace Elixir::Aether
{
    struct SParticle;
    struct SSpawnContext;
    struct SUpdateContext;
    class ParameterStore;

    enum class EModuleType : uint32_t
    {
        SpawnRate = 0,
        SetPositionDisk,
        SetVelocityCone,
        SetLifetime,
        SetSize,
        SetColor,
        ApplyGravity,
        ApplyLinearDrag,
        ColorOverLife,
        SizeOverLife,
        KillOutsideBounds,
        SetPositionOnCircle,
        SetPositionBezierLoop,
        SetPositionCircularPath,
    };

    struct SGPUModule
    {
        EModuleType Type;
        EParticleAttribute Target;
        uint32_t Parameter0Index = UINT32_MAX;
        uint32_t Parameter1Index = UINT32_MAX;
        glm::vec4 Data0{};
        glm::vec4 Data1{};
        glm::vec4 Data2{};
    };

    class ParticleSpawnModule
    {
      public:
        virtual ~ParticleSpawnModule() = default;

        virtual void Apply(SParticle& particle, SSpawnContext& context, const ParameterStore& params) const = 0;
    };

    class ParticleUpdateModule
    {
      public:
        virtual ~ParticleUpdateModule() = default;

        virtual void Apply(SParticle& particle, SUpdateContext& context, const ParameterStore& params) const = 0;
    };

    class ELIXIR_API SetPositionDisk final : public ParticleSpawnModule
    {
      public:
        explicit SetPositionDisk(glm::vec3 center, float radius, glm::vec3 normal = { 0, 1, 0 });

        void Apply(SParticle& particle, SSpawnContext& context, const ParameterStore& params) const override {}

        glm::vec3 GetCenter() const { return m_Center; }
        glm::vec3 GetNormal() const { return m_Normal; }
        float GetRadius() const { return m_Radius; }

      private:
        glm::vec3 m_Center;
        glm::vec3 m_Normal;
        float m_Radius;
    };

    class ELIXIR_API SetVelocityCone final : public ParticleSpawnModule
    {
      public:
        explicit SetVelocityCone(glm::vec3 direction, float angle, float minSpeed, float maxSpeed);

        void Apply(SParticle& particle, SSpawnContext& context, const ParameterStore& params) const override {}

        glm::vec3 GetDirection() const { return m_Direction; }
        float GetAngle() const { return m_Angle; }
        float GetMinSpeed() const { return m_MinSpeed; }
        float GetMaxSpeed() const { return m_MaxSpeed; }

      private:
        glm::vec3 m_Direction;
        float m_Angle;      // radians
        float m_MinSpeed;
        float m_MaxSpeed;
    };

    class ELIXIR_API SetLifetime final : public ParticleSpawnModule
    {
      public:
        explicit SetLifetime(float minSeconds, float maxSeconds);

        void Apply(SParticle& particle, SSpawnContext& context, const ParameterStore& params) const override {}

        float GetMinSeconds() const { return m_MinSeconds; }
        float GetMaxSeconds() const { return m_MaxSeconds; }

      private:
        float m_MinSeconds;
        float m_MaxSeconds;
    };

    class ELIXIR_API SetSize final : public ParticleSpawnModule
    {
      public:
        explicit SetSize(float minSize, float maxSize);

        void Apply(SParticle& particle, SSpawnContext& context, const ParameterStore& params) const override {}

        float GetMinSize() const { return m_MinSize; }
        float GetMaxSize() const { return m_MaxSize; }

      private:
        float m_MinSize;
        float m_MaxSize;
    };

    class ELIXIR_API SetColor final : public ParticleSpawnModule
    {
      public:
        explicit SetColor(glm::vec4 color);

        void Apply(SParticle& particle, SSpawnContext& context, const ParameterStore& params) const override {}

        const glm::vec4& GetColor() const { return m_Color; }

      private:
        glm::vec4 m_Color;
    };

    class ELIXIR_API SetPositionOnCircle final : public ParticleSpawnModule
    {
      public:
        explicit SetPositionOnCircle(glm::vec3 center, float radius, float angularSpeed, float startAngle = 0.0f);

        void Apply(SParticle& particle, SSpawnContext& context, const ParameterStore& params) const override {}

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

        void Apply(SParticle& particle, SSpawnContext& context, const ParameterStore& params) const override {}

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

    class ELIXIR_API ApplyGravity final : public ParticleUpdateModule
    {
      public:
        explicit ApplyGravity(glm::vec3 gravity);

        void Apply(SParticle& particle, SUpdateContext& context, const ParameterStore& params) const override {}

        const glm::vec3& GetGravity() const { return m_Gravity; }

      private:
        glm::vec3 m_Gravity;
    };

    class ELIXIR_API ApplyLinearDrag final : public ParticleUpdateModule
    {
      public:
        explicit ApplyLinearDrag(float dragPerSecond);

        void Apply(SParticle& particle, SUpdateContext& context, const ParameterStore& params) const override {}

        float GetDragPerSecond() const { return m_DragPerSecond; }

      private:
        float m_DragPerSecond;
    };

    class ELIXIR_API ColorOverLife final : public ParticleUpdateModule
    {
      public:
        explicit ColorOverLife(glm::vec4 startColor, glm::vec4 endColor);

        void Apply(SParticle& particle, SUpdateContext& context, const ParameterStore& params) const override {}

        const glm::vec4& GetStartColor() const { return m_StartColor; }
        const glm::vec4& GetEndColor() const { return m_EndColor; }

      private:
        glm::vec4 m_StartColor;
        glm::vec4 m_EndColor;
    };

    class ELIXIR_API SizeOverLife final : public ParticleUpdateModule
    {
    public:
        SizeOverLife(float startSize, float endSize);

        void Apply(SParticle& particle, SUpdateContext& context, const ParameterStore& params) const override {}

        float GetStartSize() const { return m_StartSize; }
        float GetEndSize() const { return m_EndSize; }

    private:
        float m_StartSize;
        float m_EndSize;
    };

    class ELIXIR_API KillOutsideBounds final : public ParticleUpdateModule
    {
      public:
        explicit KillOutsideBounds(glm::vec3 min, glm::vec3 max);

        void Apply(SParticle& particle, SUpdateContext& context, const ParameterStore& params) const override {}

        glm::vec3 GetMin() const { return m_Min; }
        glm::vec3 GetMax() const { return m_Max; }

      private:
        glm::vec3 m_Min;
        glm::vec3 m_Max;
    };
}