#pragma once

#include <utility>

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Sets spawn velocities within a cone. */
    class ELIXIR_API SetVelocityCone final : public SpawnModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit SetVelocityCone(glm::vec3 direction, float angle, float minSpeed, float maxSpeed)
            : m_Direction(direction),
            m_Angle(angle),
            m_MinSpeed(minSpeed),
            m_MaxSpeed(maxSpeed) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            const uint32_t angleIndex = context.FindParameter(m_AngleParamName);
            context.Emit({
                Core::EParticleOp::SampleCone,
                Core::EParticleAttribute::Velocity,
                context.FindParameter(m_MinSpeedParamName),
                context.FindParameter(m_MaxSpeedParamName),
                { m_Direction, m_Angle },
                {
                    m_MinSpeed, m_MaxSpeed,
                    (float)(angleIndex == UINT32_MAX ? -1 : (int32_t)angleIndex), 0.0f
                }
            });
        }

        /** @brief Binds named parameters to the configured values and returns this module. */
        SetVelocityCone& BindParameters(std::string angleParam, std::string minSpeedParam, std::string maxSpeedParam)
        {
            m_AngleParamName = std::move(angleParam);
            m_MinSpeedParamName = std::move(minSpeedParam);
            m_MaxSpeedParamName = std::move(maxSpeedParam);
            return *this;
        }

        /** @brief Returns the configured direction. */
        glm::vec3 GetDirection() const { return m_Direction; }

        /** @brief Returns the cone angle in radians. */
        float GetAngle() const { return m_Angle; }

        /** @brief Returns the configured minimum speed. */
        float GetMinSpeed() const { return m_MinSpeed; }

        /** @brief Returns the configured maximum speed. */
        float GetMaxSpeed() const { return m_MaxSpeed; }

        /** @brief Returns the configured angle parameter name. */
        const std::string& GetAngleParamName() const { return m_AngleParamName; }

        /** @brief Returns the configured minimum speed parameter name. */
        const std::string& GetMinSpeedParamName() const { return m_MinSpeedParamName; }

        /** @brief Returns the configured maximum speed parameter name. */
        const std::string& GetMaxSpeedParamName() const { return m_MaxSpeedParamName; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const glm::vec3 dir = context.RequireFloatVec<3>(object, "direction");
            const auto angle = context.ParseScalar(object, "angle");
            const auto minSpeed = context.ParseScalar(object, "minSpeed");
            const auto maxSpeed = context.ParseScalar(object, "maxSpeed");
            auto module = CreateScope<SetVelocityCone>(dir, angle.Value, minSpeed.Value, maxSpeed.Value);
            module->BindParameters(angle.Param, minSpeed.Param, maxSpeed.Param);
            return module;
        }

    private:
        glm::vec3 m_Direction;
        float m_Angle;      // radians
        float m_MinSpeed;
        float m_MaxSpeed;
        std::string m_AngleParamName;
        std::string m_MinSpeedParamName;
        std::string m_MaxSpeedParamName;
    };
}
