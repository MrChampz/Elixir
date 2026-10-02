#pragma once

#include <utility>

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Sets initial particle scales within a range. */
    class ELIXIR_API SetScale final : public SpawnModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit SetScale(float minScale, float maxScale)
            : m_MinScale(minScale), m_MaxScale(maxScale) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::RandomRange,
                Core::EParticleAttribute::Scale,
                context.FindParameter(m_MinScaleParamName),
                context.FindParameter(m_MaxScaleParamName),
                { m_MinScale, 0.0f, 0.0f, 0.0f },
                { m_MaxScale, 0.0f, 0.0f, 0.0f }
            });
        }

        /** @brief Binds named parameters to the configured values and returns this module. */
        SetScale& BindParameters(std::string minScaleParam, std::string maxScaleParam)
        {
            m_MinScaleParamName = std::move(minScaleParam);
            m_MaxScaleParamName = std::move(maxScaleParam);
            return *this;
        }

        /** @brief Returns the configured minimum scale. */
        float GetMinScale() const { return m_MinScale; }

        /** @brief Returns the configured maximum scale. */
        float GetMaxScale() const { return m_MaxScale; }

        /** @brief Returns the configured minimum scale parameter name. */
        const std::string& GetMinScaleParamName() const { return m_MinScaleParamName; }

        /** @brief Returns the configured maximum scale parameter name. */
        const std::string& GetMaxScaleParamName() const { return m_MaxScaleParamName; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const auto low = context.ParseScalar(object, "min");
            const auto high = context.ParseScalar(object, "max");
            auto module = CreateScope<SetScale>(low.Value, high.Value);
            module->BindParameters(low.Param, high.Param);
            return module;
        }

    private:
        float m_MinScale;
        float m_MaxScale;
        std::string m_MinScaleParamName;
        std::string m_MaxScaleParamName;
    };
}
