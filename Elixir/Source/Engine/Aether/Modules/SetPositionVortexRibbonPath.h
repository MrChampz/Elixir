#pragma once

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Sets spawn positions along a pulsing vortex ribbon path. */
    class ELIXIR_API SetPositionVortexRibbonPath final : public SpawnModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit SetPositionVortexRibbonPath(
            glm::vec3 center,
            float orbitSpeed,
            float baseRadius,
            float radiusAmplitude,
            float radiusSpeed,
            float pulseAmplitude,
            float pulseSpeed,
            float curlAmplitude,
            float depthAmplitude
        ) : m_Center(center),
            m_OrbitSpeed(orbitSpeed),
            m_BaseRadius(baseRadius),
            m_RadiusAmplitude(radiusAmplitude),
            m_RadiusSpeed(radiusSpeed),
            m_PulseAmplitude(pulseAmplitude),
            m_PulseSpeed(pulseSpeed),
            m_CurlAmplitude(curlAmplitude),
            m_DepthAmplitude(depthAmplitude) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::SetPositionVortexRibbonPath,
                Core::EParticleAttribute::Position,
                UINT32_MAX,
                UINT32_MAX,
                { m_Center, 0.0 },
                {
                    m_OrbitSpeed,
                    m_BaseRadius,
                    m_RadiusAmplitude,
                    m_RadiusSpeed
                },
                {
                    m_PulseAmplitude,
                    m_PulseSpeed,
                    m_CurlAmplitude,
                    m_DepthAmplitude
                }
            });
        }

        /** @brief Returns the configured center. */
        glm::vec3 GetCenter() const { return m_Center; }

        /** @brief Returns the configured orbit speed. */
        float GetOrbitSpeed() const { return m_OrbitSpeed; }

        /** @brief Returns the configured base radius. */
        float GetBaseRadius() const { return m_BaseRadius; }

        /** @brief Returns the configured radius amplitude. */
        float GetRadiusAmplitude() const { return m_RadiusAmplitude; }

        /** @brief Returns the configured radius speed. */
        float GetRadiusSpeed() const { return m_RadiusSpeed; }

        /** @brief Returns the configured pulse amplitude. */
        float GetPulseAmplitude() const { return m_PulseAmplitude; }

        /** @brief Returns the configured pulse speed. */
        float GetPulseSpeed() const { return m_PulseSpeed; }

        /** @brief Returns the configured curl amplitude. */
        float GetCurlAmplitude() const { return m_CurlAmplitude; }

        /** @brief Returns the configured depth amplitude. */
        float GetDepthAmplitude() const { return m_DepthAmplitude; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const glm::vec3 center = context.RequireFloatVec<3>(object, "center");
            const float orbitSpeed = context.RequireFloat(object, "orbitSpeed");
            const float baseRadius = context.RequireFloat(object, "baseRadius");
            const float radiusAmplitude = context.RequireFloat(object, "radiusAmplitude");
            const float radiusSpeed = context.RequireFloat(object, "radiusSpeed");
            const float pulseAmplitude = context.RequireFloat(object, "pulseAmplitude");
            const float pulseSpeed = context.RequireFloat(object, "pulseSpeed");
            const float curlAmplitude = context.RequireFloat(object, "curlAmplitude");
            const float depthAmplitude = context.RequireFloat(object, "depthAmplitude");

            auto module = CreateScope<SetPositionVortexRibbonPath>(center,
                orbitSpeed,
                baseRadius,
                radiusAmplitude,
                radiusSpeed,
                pulseAmplitude,
                pulseSpeed,
                curlAmplitude,
                depthAmplitude
            );
            return module;
        }

    private:
        glm::vec3 m_Center;
        float m_OrbitSpeed;
        float m_BaseRadius;
        float m_RadiusAmplitude;
        float m_RadiusSpeed;
        float m_PulseAmplitude;
        float m_PulseSpeed;
        float m_CurlAmplitude;
        float m_DepthAmplitude;
    };
}
