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
        explicit SetRibbonId(uint32_t ribbonId)
            : m_RibbonId(ribbonId) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::SetLiteral,
                Core::EParticleAttribute::RibbonId,
                UINT32_MAX,
                UINT32_MAX,
                { (float)m_RibbonId, 0.0f, 0.0f, 0.0f }
            });
        }

        /** @brief Returns the configured ribbon id. */
        uint32_t GetRibbonId() const { return m_RibbonId; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            auto module = CreateScope<SetRibbonId>(context.RequireUInt(object, "ribbonId"));
            return module;
        }

    private:
        uint32_t m_RibbonId;
    };
}
