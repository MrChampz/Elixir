#pragma once

#include <utility>

#include <Engine/Aether/Modules/DynamicInput.h>
#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Applies angular velocity to particle rotation. */
    class ELIXIR_API ApplyAngularVelocity final : public UpdateModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit ApplyAngularVelocity(float radiansPerSecond)
            : m_RadiansPerSecond(radiansPerSecond) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::AddWithDelta,
                Core::EParticleAttribute::Rotation,
                context.FindParameter(m_ParamName),
                UINT32_MAX,
                { m_RadiansPerSecond, 0.0f, 0.0f, 0.0f },
                { (float)((uint32_t)m_Input), 0.0f, 0.0f, 0.0f }
            });
        }

        /** @brief Binds a named parameter to the configured value and returns this module. */
        ApplyAngularVelocity& BindParameter(std::string paramName)
        {
            m_ParamName = std::move(paramName);
            return *this;
        }

        /** @brief Selects the dynamic input for angular velocity and returns this module. */
        ApplyAngularVelocity& BindInput(EDynamicInput input)
        {
            m_Input = input;
            return *this;
        }

        /** @brief Returns the angular velocity in radians per second. */
        float GetRadiansPerSecond() const { return m_RadiansPerSecond; }

        /** @brief Returns the configured parameter name. */
        const std::string& GetParamName() const { return m_ParamName; }

        /** @brief Returns the dynamic input used for angular velocity. */
        EDynamicInput GetInput() const { return m_Input; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const auto value = context.ParseScalar(object, "value");
            auto module = CreateScope<ApplyAngularVelocity>(value.Value);
            module->BindParameter(value.Param);
            if (context.HasField(object, "input"))
                module->BindInput(context.ParseDynamicInput(object, "input"));
            return module;
        }

    private:
        float m_RadiansPerSecond;
        std::string m_ParamName;
        EDynamicInput m_Input = EDynamicInput::None;
    };
}
