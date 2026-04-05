#pragma once

namespace Elixir::Aether
{
    struct SParticle;
    struct SSpawnContext;
    struct SUpdateContext;
    class ParameterStore;

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

    class ELIXIR_API SetPositionCircle final : public ParticleSpawnModule
    {
      public:
        explicit SetPositionCircle(glm::vec2 center, float radius);

        void Apply(SParticle& particle, SSpawnContext& context, const ParameterStore& params) const override;

      private:
        glm::vec2 m_Center;
        float m_Radius;
    };

    class ELIXIR_API SetVelocityCone final : public ParticleSpawnModule
    {
      public:
        explicit SetVelocityCone(float minAngle, float maxAngle, float minSpeed, float maxSpeed);

        void Apply(SParticle& particle, SSpawnContext& context, const ParameterStore& params) const override;

      private:
        float m_MinAngle; // radians
        float m_MaxAngle; // radians
        float m_MinSpeed;
        float m_MaxSpeed;
    };

    class ELIXIR_API SetLifetime final : public ParticleSpawnModule
    {
      public:
        explicit SetLifetime(float minSeconds, float maxSeconds);

        void Apply(SParticle& particle, SSpawnContext& context, const ParameterStore& params) const override;

      private:
        float m_MinSeconds;
        float m_MaxSeconds;
    };

    class ELIXIR_API SetSize final : public ParticleSpawnModule
    {
      public:
        explicit SetSize(float minSize, float maxSize);

        void Apply(SParticle& particle, SSpawnContext& context, const ParameterStore& params) const override;

      private:
        float m_MinSize;
        float m_MaxSize;
    };

    class ELIXIR_API SetColor final : public ParticleSpawnModule
    {
      public:
        explicit SetColor(glm::vec4 color);

        void Apply(SParticle& particle, SSpawnContext& context, const ParameterStore& params) const override;

      private:
        glm::vec4 m_Color;
    };

    class ELIXIR_API ApplyGravity final : public ParticleUpdateModule
    {
      public:
        explicit ApplyGravity(glm::vec2 gravity);

        void Apply(SParticle& particle, SUpdateContext& context, const ParameterStore& params) const override;

      private:
        glm::vec2 m_Gravity;
    };

    class ELIXIR_API ApplyLinearDrag final : public ParticleUpdateModule
    {
      public:
        explicit ApplyLinearDrag(float dragPerSecond);

        void Apply(SParticle& particle, SUpdateContext& context, const ParameterStore& params) const override;

      private:
        float m_DragPerSecond;
    };

    class ELIXIR_API IntegrateVelocity final : public ParticleUpdateModule
    {
      public:
        void Apply(SParticle& particle, SUpdateContext& context, const ParameterStore& params) const override;
    };

    class ELIXIR_API ColorOverLife final : public ParticleUpdateModule
    {
      public:
        explicit ColorOverLife(glm::vec4 startColor, glm::vec4 endColor);

        void Apply(SParticle& particle, SUpdateContext& context, const ParameterStore& params) const override;

      private:
        glm::vec4 m_StartColor;
        glm::vec4 m_EndColor;
    };

    class ELIXIR_API SizeOverLife final : public ParticleUpdateModule
    {
    public:
        SizeOverLife(float startSize, float endSize);

        void Apply(SParticle& particle, SUpdateContext& context, const ParameterStore& params) const override;

    private:
        float m_StartSize;
        float m_EndSize;
    };

    class ELIXIR_API KillOutsideBounds final : public ParticleUpdateModule
    {
      public:
        explicit KillOutsideBounds(glm::vec2 min, glm::vec2 max);

        void Apply(SParticle& particle, SUpdateContext& context, const ParameterStore& params) const override;

      private:
        glm::vec2 m_Min;
        glm::vec2 m_Max;
    };
}