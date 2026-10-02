#pragma once

#include <algorithm>

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Assigns ribbon identifiers from particle spawn order. */
    class ELIXIR_API SetRibbonIdFromSpawnOrder final : public SpawnModule
    {
    public:
        /** @brief Creates the module, clamping the ribbon count to at least one. */
        explicit SetRibbonIdFromSpawnOrder(uint32_t ribbonCount, uint32_t firstRibbonId = 0)
            : m_RibbonCount(std::max(1u, ribbonCount)),
            m_FirstRibbonId(firstRibbonId) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::SetRibbonIdFromSpawnOrder,
                Core::EParticleAttribute::RibbonId,
                UINT32_MAX,
                UINT32_MAX,
                {
                    (float)m_RibbonCount,
                    (float)m_FirstRibbonId,
                    0.0f, 0.0f
                }
            });
        }

        /** @brief Returns the ribbon count, which is at least one. */
        uint32_t GetRibbonCount() const { return m_RibbonCount; }

        /** @brief Returns the configured first ribbon id. */
        uint32_t GetFirstRibbonId() const { return m_FirstRibbonId; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const uint32_t count = context.RequireUInt(object, "ribbonCount");
            const uint32_t first = context.RequireUInt(object, "firstRibbonId");
            auto module = CreateScope<SetRibbonIdFromSpawnOrder>(count, first);
            return module;
        }

    private:
        uint32_t m_RibbonCount;
        uint32_t m_FirstRibbonId;
    };
}
