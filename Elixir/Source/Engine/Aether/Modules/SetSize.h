#pragma once

#include <utility>

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Sets particle sizes within a range. */
    class ELIXIR_API SetSize final : public SpawnModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit SetSize(float minSize, float maxSize)
            : m_MinSize(minSize), m_MaxSize(maxSize) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::RandomRange,
                Core::EParticleAttribute::Size,
                context.FindParameter(m_MinSizeParamName),
                context.FindParameter(m_MaxSizeParamName),
                { m_MinSize, 0.0f, 0.0f, 0.0f },
                { m_MaxSize, 0.0f, 0.0f, 0.0f }
            });
        }

        /** @brief Binds named parameters to the configured values and returns this module. */
        SetSize& BindParameters(std::string minSizeParam, std::string maxSizeParam)
        {
            m_MinSizeParamName = std::move(minSizeParam);
            m_MaxSizeParamName = std::move(maxSizeParam);
            return *this;
        }

        /** @brief Returns the configured minimum size. */
        float GetMinSize() const { return m_MinSize; }

        /** @brief Returns the configured maximum size. */
        float GetMaxSize() const { return m_MaxSize; }

        /** @brief Returns the configured minimum size parameter name. */
        const std::string& GetMinSizeParamName() const { return m_MinSizeParamName; }

        /** @brief Returns the configured maximum size parameter name. */
        const std::string& GetMaxSizeParamName() const { return m_MaxSizeParamName; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const auto low = context.ParseScalar(object, "min");
            const auto high = context.ParseScalar(object, "max");
            auto module = CreateScope<SetSize>(low.Value, high.Value);
            module->BindParameters(low.Param, high.Param);
            return module;
        }

    private:
        float m_MinSize;
        float m_MaxSize;
        std::string m_MinSizeParamName;
        std::string m_MaxSizeParamName;
    };
}
