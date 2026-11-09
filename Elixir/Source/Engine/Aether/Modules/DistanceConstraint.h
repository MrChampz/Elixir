#pragma once

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    using namespace Effect;

    /** Constrains distances between live particles with matching emitter and GroupId. */
    class ELIXIR_API DistanceConstraint final : public UpdateModule
    {
    public:
        /**
         * @brief Creates a distance constraint for ordered particle chains.
         * @param config Distance, stiffness, topology, and anchoring values.
         */
        explicit DistanceConstraint(const SDistanceConstraint& config = {})
            : m_Config(config) {}

        /**
         * @brief Schedules this update module after particle position integration.
         * @param context Destination compilation context.
         */
        void Compile(ModuleCompileContext& context) const override
        {
            context.EmitDistanceConstraint(m_Config);
        }

        /** @brief Returns the authored distance constraint values. */
        const SDistanceConstraint& GetConfig() const { return m_Config; }

        /**
         * @brief Creates a distance constraint from its serialized fields.
         * @param context Parser reporting invalid values.
         * @param object Serialized module definition.
         * @return The module, or null when its configuration is invalid.
         */
        static Scope<Module> Create(
            ModuleParseContext& context,
            simdjson::ondemand::object& object
        );

    private:
        SDistanceConstraint m_Config;
    };
}
