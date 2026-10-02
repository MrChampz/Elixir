#pragma once

#include <utility>

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Changes particle size over its lifetime. */
    class ELIXIR_API SizeOverLife final : public UpdateModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        SizeOverLife(float startSize, float endSize)
            : m_StartSize(startSize), m_EndSize(endSize) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::LerpOverLife,
                Core::EParticleAttribute::Size,
                context.FindParameter(m_StartSizeParamName),
                context.FindParameter(m_EndSizeParamName),
                { m_StartSize, 0.0f, 0.0f, 0.0f },
                { m_EndSize, 0.0f, 0.0f, 0.0f }
            });
        }

        /** @brief Binds named parameters to the configured values and returns this module. */
        SizeOverLife& BindParameters(std::string startSizeParam, std::string endSizeParam)
        {
            m_StartSizeParamName = std::move(startSizeParam);
            m_EndSizeParamName = std::move(endSizeParam);
            return *this;
        }

        /** @brief Returns the configured start size. */
        float GetStartSize() const { return m_StartSize; }

        /** @brief Returns the configured end size. */
        float GetEndSize() const { return m_EndSize; }

        /** @brief Returns the configured start size parameter name. */
        const std::string& GetStartSizeParamName() const { return m_StartSizeParamName; }

        /** @brief Returns the configured end size parameter name. */
        const std::string& GetEndSizeParamName() const { return m_EndSizeParamName; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const auto low = context.ParseScalar(object, "start");
            const auto high = context.ParseScalar(object, "end");
            auto module = CreateScope<SizeOverLife>(low.Value, high.Value);
            module->BindParameters(low.Param, high.Param);
            return module;
        }

    private:
        float m_StartSize;
        float m_EndSize;
        std::string m_StartSizeParamName;
        std::string m_EndSizeParamName;
    };
}
