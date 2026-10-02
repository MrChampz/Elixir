#pragma once

#include <utility>

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Applies linear drag to particle velocity. */
    class ELIXIR_API ApplyLinearDrag final : public UpdateModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit ApplyLinearDrag(float dragPerSecond)
            : m_DragPerSecond(dragPerSecond) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::Dampen,
                Core::EParticleAttribute::Velocity,
                context.FindParameter(m_ParamName),
                UINT32_MAX,
                { m_DragPerSecond, 0.0f, 0.0f, 0.0f },
                {}
            });
        }

        /** @brief Binds a named parameter to the configured value and returns this module. */
        ApplyLinearDrag& BindParameter(std::string paramName)
        {
            m_ParamName = std::move(paramName);
            return *this;
        }

        /** @brief Returns the configured drag per second. */
        float GetDragPerSecond() const { return m_DragPerSecond; }

        /** @brief Returns the configured parameter name. */
        const std::string& GetParamName() const { return m_ParamName; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const auto drag = context.ParseScalar(object, "drag");
            auto module = CreateScope<ApplyLinearDrag>(drag.Value);
            module->BindParameter(drag.Param);
            return module;
        }

    private:
        float m_DragPerSecond;
        std::string m_ParamName;
    };
}
