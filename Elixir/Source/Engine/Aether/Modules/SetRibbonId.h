#pragma once

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Assigns a fixed ribbon identifier to spawned particles. */
    class ELIXIR_API SetRibbonId final : public SpawnModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit SetRibbonId(const uint32_t ribbonId)
            : m_RibbonId(ribbonId) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit(
                EOperationType::SetLiteral,
                EParticleAttribute::RibbonId,
                {
                    context.UInt(m_RibbonId, m_ParamName)
                }
            );
        }

        /** @brief Returns the configured ribbon id. */
        uint32_t GetRibbonId() const { return m_RibbonId; }

        /** @brief Binds a named UInt parameter and returns this module. */
        SetRibbonId& BindParameter(std::string name)
        {
            m_ParamName = std::move(name);
            return *this;
        }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(
            Effect::ModuleParseContext& context,
            simdjson::ondemand::object& object
        )
        {
            const auto ribbonId = context.RequireUInt(object, "ribbonId");
            auto module = CreateScope<SetRibbonId>(ribbonId);
            return module;
        }

    private:
        uint32_t m_RibbonId;
        std::string m_ParamName;
    };
}
