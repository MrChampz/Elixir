#pragma once

#include <utility>

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Applies gravity to particle velocity. */
    class ELIXIR_API ApplyGravity final : public UpdateModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit ApplyGravity(glm::vec3 gravity)
            : m_Gravity(gravity) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::AddWithDelta,
                Core::EParticleAttribute::Velocity,
                context.FindParameter(m_ParamName),
                UINT32_MAX,
                { m_Gravity * context.GetGravityScale(), 0.0f },
                {}
            });
        }

        /** @brief Binds a named parameter to the configured value and returns this module. */
        ApplyGravity& BindParameter(std::string paramName)
        {
            m_ParamName = std::move(paramName);
            return *this;
        }

        /** @brief Returns the configured gravity. */
        const glm::vec3& GetGravity() const { return m_Gravity; }

        /** @brief Returns the configured parameter name. */
        const std::string& GetParamName() const { return m_ParamName; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const auto gravity = context.ParseFloat4(object, "gravity");
            auto module = CreateScope<ApplyGravity>(glm::vec3{ gravity.Value });
            module->BindParameter(gravity.Param);
            return module;
        }

    private:
        glm::vec3 m_Gravity;
        std::string m_ParamName;
    };
}
