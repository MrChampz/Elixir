#pragma once

#include <utility>

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Sets the initial particle color. */
    class ELIXIR_API SetColor final : public SpawnModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit SetColor(glm::vec4 color)
            : m_Color(color) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::SetLiteral,
                Core::EParticleAttribute::Color,
                context.FindParameter(m_ParamName),
                UINT32_MAX,
                m_Color,
            });
        }

        /** @brief Binds a named parameter to the configured value and returns this module. */
        SetColor& BindParameter(std::string paramName)
        {
            m_ParamName = std::move(paramName);
            return *this;
        }

        /** @brief Returns the configured color. */
        const glm::vec4& GetColor() const { return m_Color; }

        /** @brief Returns the configured parameter name. */
        const std::string& GetParamName() const { return m_ParamName; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const auto color = context.ParseFloat4(object, "color", glm::vec4(1.0f));
            auto module = CreateScope<SetColor>(color.Value);
            module->BindParameter(color.Param);
            return module;
        }

    private:
        glm::vec4 m_Color;
        std::string m_ParamName;
    };
}
