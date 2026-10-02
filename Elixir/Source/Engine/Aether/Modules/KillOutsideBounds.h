#pragma once

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Kills particles outside a box. */
    class ELIXIR_API KillOutsideBounds final : public UpdateModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit KillOutsideBounds(glm::vec3 min, glm::vec3 max)
            : m_Min(min), m_Max(max) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::KillOutsideBounds,
                Core::EParticleAttribute::Position,
                UINT32_MAX,
                UINT32_MAX,
                { m_Min, 0.0f },
                { m_Max, 0.0f }
            });
        }

        /** @brief Returns the configured min. */
        glm::vec3 GetMin() const { return m_Min; }

        /** @brief Returns the configured max. */
        glm::vec3 GetMax() const { return m_Max; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const glm::vec3 min = context.RequireFloatVec<3>(object, "min");
            const glm::vec3 max = context.RequireFloatVec<3>(object, "max");
            auto module = CreateScope<KillOutsideBounds>(min, max);
            return module;
        }

    private:
        glm::vec3 m_Min;
        glm::vec3 m_Max;
    };
}
