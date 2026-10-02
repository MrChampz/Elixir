#pragma once

#include <utility>

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Sets initial particle rotations within a range in radians. */
    class ELIXIR_API SetRotation final : public SpawnModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit SetRotation(float minRotation, float maxRotation)
            : m_MinRotation(minRotation), m_MaxRotation(maxRotation) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::RandomRange,
                Core::EParticleAttribute::Rotation,
                context.FindParameter(m_MinRotationParamName),
                context.FindParameter(m_MaxRotationParamName),
                { m_MinRotation, 0.0f, 0.0f, 0.0f },
                { m_MaxRotation, 0.0f, 0.0f, 0.0f }
            });
        }

        /** @brief Binds named parameters to the configured values and returns this module. */
        SetRotation& BindParameters(std::string minRotationParam, std::string maxRotationParam)
        {
            m_MinRotationParamName = std::move(minRotationParam);
            m_MaxRotationParamName = std::move(maxRotationParam);
            return *this;
        }

        /** @brief Returns the minimum initial rotation in radians. */
        float GetMinRotation() const { return m_MinRotation; }

        /** @brief Returns the maximum initial rotation in radians. */
        float GetMaxRotation() const { return m_MaxRotation; }

        /** @brief Returns the configured minimum rotation parameter name. */
        const std::string& GetMinRotationParamName() const { return m_MinRotationParamName; }

        /** @brief Returns the configured maximum rotation parameter name. */
        const std::string& GetMaxRotationParamName() const { return m_MaxRotationParamName; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const auto low = context.ParseScalar(object, "min");
            const auto high = context.ParseScalar(object, "max");
            auto module = CreateScope<SetRotation>(low.Value, high.Value);
            module->BindParameters(low.Param, high.Param);
            return module;
        }

    private:
        float m_MinRotation;
        float m_MaxRotation;
        std::string m_MinRotationParamName;
        std::string m_MaxRotationParamName;

    };
}
