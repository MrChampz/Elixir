#pragma once

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /**
     * Assigns particle groups from spawn order for Ribbon and Strand renderers.
     * Groups repeat in round-robin order. LinkOrder remains the particle spawn order.
     */
    class ELIXIR_API SetGroupIdFromSpawnOrder final : public SpawnModule
    {
    public:
        /**
         * @brief Creates a module that distributes spawned particles across groups.
         * @param groupCount Number of interleaved groups, clamped to at least one.
         * @param firstGroupId Identifier assigned to the first group.
         */
        explicit SetGroupIdFromSpawnOrder(
            const uint32_t groupCount,
            const uint32_t firstGroupId = 0
        ) : m_GroupCount(std::max(1u, groupCount)),
            m_FirstGroupId(firstGroupId) {}

        /**
         * @brief Appends this module's GPU operations to the supplied context.
         * @param context Destination compilation context.
         */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit(
                EOperationType::SetGroupIdFromSpawnOrder,
                EParticleAttribute::GroupId,
                {
                    context.UInt(m_GroupCount),
                    context.UInt(m_FirstGroupId)
                }
            );
        }

        /** @brief Returns the group count, which is at least one. */
        uint32_t GetGroupCount() const { return m_GroupCount; }

        /** @brief Returns the configured first group id. */
        uint32_t GetFirstGroupId() const { return m_FirstGroupId; }

        /**
         * @brief Creates this module from its serialized fields.
         * @param context Parser reporting invalid values.
         * @param object Serialized module definition.
         * @return The module created from the definition.
         */
        static Scope<Module> Create(
            Effect::ModuleParseContext& context,
            simdjson::ondemand::object& object
        )
        {
            const uint32_t count = context.RequireUInt(object, "groupCount");
            const uint32_t first = context.RequireUInt(object, "firstGroupId");
            auto module = CreateScope<SetGroupIdFromSpawnOrder>(count, first);
            return module;
        }

    private:
        uint32_t m_GroupCount;
        uint32_t m_FirstGroupId;
    };
}
