#pragma once

#include <utility>

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Sets particle lifetimes within a range in seconds. */
    class ELIXIR_API SetLifetime final : public SpawnModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit SetLifetime(float minSeconds, float maxSeconds)
            : m_MinSeconds(minSeconds), m_MaxSeconds(maxSeconds) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::RandomRange,
                Core::EParticleAttribute::Lifetime,
                context.FindParameter(m_MinSecondsParamName),
                context.FindParameter(m_MaxSecondsParamName),
                { m_MinSeconds, 0.0f, 0.0f, 0.0f },
                { m_MaxSeconds, 0.0f, 0.0f, 0.0f }
            });
        }

        /** @brief Binds named parameters to the configured values and returns this module. */
        SetLifetime& BindParameters(std::string minSecondsParam, std::string maxSecondsParam)
        {
            m_MinSecondsParamName = std::move(minSecondsParam);
            m_MaxSecondsParamName = std::move(maxSecondsParam);
            return *this;
        }

        /** @brief Returns the minimum lifetime in seconds. */
        float GetMinSeconds() const { return m_MinSeconds; }

        /** @brief Returns the maximum lifetime in seconds. */
        float GetMaxSeconds() const { return m_MaxSeconds; }

        /** @brief Returns the configured minimum lifetime parameter name. */
        const std::string& GetMinSecondsParamName() const { return m_MinSecondsParamName; }

        /** @brief Returns the configured maximum lifetime parameter name. */
        const std::string& GetMaxSecondsParamName() const { return m_MaxSecondsParamName; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const auto low = context.ParseScalar(object, "min");
            const auto high = context.ParseScalar(object, "max");
            auto module = CreateScope<SetLifetime>(low.Value, high.Value);
            module->BindParameters(low.Param, high.Param);
            return module;
        }

    private:
        float m_MinSeconds;
        float m_MaxSeconds;
        std::string m_MinSecondsParamName;
        std::string m_MaxSecondsParamName;
    };
}
