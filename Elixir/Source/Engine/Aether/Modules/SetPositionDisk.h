#pragma once

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Sets spawn positions within a disk. */
    class ELIXIR_API SetPositionDisk final : public SpawnModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit SetPositionDisk(glm::vec3 center, float radius, glm::vec3 normal = { 0, 1, 0 })
            : m_Center(center), m_Normal(normal), m_Radius(radius) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::SampleDisk,
                Core::EParticleAttribute::Position,
                UINT32_MAX,
                UINT32_MAX,
                { m_Center, m_Radius },
                { m_Normal, 0.0f }
            });
        }

        /** @brief Returns the configured center. */
        glm::vec3 GetCenter() const { return m_Center; }

        /** @brief Returns the configured normal. */
        glm::vec3 GetNormal() const { return m_Normal; }

        /** @brief Returns the configured radius. */
        float GetRadius() const { return m_Radius; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const glm::vec3 center = context.RequireFloatVec<3>(object, "center");
            const float radius = context.RequireFloat(object, "radius");
            if (context.HasField(object, "normal"))
            {
                const auto normal = context.RequireFloatVec<3>(object, "normal");
                auto module = CreateScope<SetPositionDisk>(center, radius, normal);
                return module;
            }
            else
            {
                auto module = CreateScope<SetPositionDisk>(center, radius);
                return module;
            }
        }

    private:
        glm::vec3 m_Center;
        glm::vec3 m_Normal;
        float m_Radius;
    };
}
