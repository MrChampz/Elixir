#pragma once

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Sets spawn positions on a moving point along a circle. */
    class ELIXIR_API SetPositionOnCircle final : public SpawnModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit SetPositionOnCircle(glm::vec3 center, float radius, float angularSpeed, float startAngle = 0.0f)
            : m_Center(center),
            m_Radius(radius),
            m_AngularSpeed(angularSpeed),
            m_StartAngle(startAngle) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::SetPositionOnCircle,
                Core::EParticleAttribute::Position,
                UINT32_MAX,
                UINT32_MAX,
                { m_Center, m_Radius },
                { m_AngularSpeed, m_StartAngle, 0.0, 0.0 }
            });
        }

        /** @brief Returns the configured center. */
        glm::vec3 GetCenter() const { return m_Center; }

        /** @brief Returns the configured radius. */
        float GetRadius() const { return m_Radius; }

        /** @brief Returns the angular speed in radians per second. */
        float GetAngularSpeed() const { return m_AngularSpeed; }

        /** @brief Returns the starting angle in radians. */
        float GetStartAngle() const { return m_StartAngle; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const glm::vec3 center = context.RequireFloatVec<3>(object, "center");
            const float radius = context.RequireFloat(object, "radius");
            const float angularSpeed = context.RequireFloat(object, "angularSpeed");
            const float startAngle = context.HasField(object, "startAngle")
                ? context.RequireFloat(object, "startAngle") : 0.0f;
            return CreateScope<SetPositionOnCircle>(center, radius, angularSpeed, startAngle);
        }

    private:
        glm::vec3 m_Center;
        float m_Radius;
        float m_AngularSpeed;
        float m_StartAngle;
    };
}
