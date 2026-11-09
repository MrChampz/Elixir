#pragma once

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** Assigns a shared particle group used by Ribbon and Strand renderers. */
    class ELIXIR_API SetGroupId final : public SpawnModule
    {
    public:
        /**
         * @brief Creates a module that assigns a fixed particle group.
         * @param groupId Identifier shared by particles in the same group.
         */
        explicit SetGroupId(const uint32_t groupId)
            : m_GroupId(groupId) {}

        /**
         * @brief Appends this module's GPU operations to the supplied context.
         * @param context Destination compilation context.
         */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit(
                EOperationType::SetLiteral,
                EParticleAttribute::GroupId,
                {
                    context.UInt(m_GroupId, m_ParamName)
                }
            );
        }

        /** @brief Returns the configured group id. */
        uint32_t GetGroupId() const { return m_GroupId; }

        /**
         * @brief Binds a named UInt parameter and returns this module.
         * @param name Parameter containing the group identifier.
         */
        SetGroupId& BindParameter(std::string name)
        {
            m_ParamName = std::move(name);
            return *this;
        }

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
            const auto groupId = context.RequireUInt(object, "groupId");
            auto module = CreateScope<SetGroupId>(groupId);
            return module;
        }

    private:
        uint32_t m_GroupId;
        std::string m_ParamName;
    };
}
