#pragma once

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Sets spawn positions within a box. */
    class ELIXIR_API SetPositionBox final : public SpawnModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit SetPositionBox(glm::vec3 minBounds, glm::vec3 maxBounds)
            : m_MinBounds(minBounds), m_MaxBounds(maxBounds) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit({
                Core::EParticleOp::SampleBox,
                Core::EParticleAttribute::Position,
                UINT32_MAX,
                UINT32_MAX,
                { m_MinBounds, 0.0f },
                { m_MaxBounds, 0.0f }
            });
        }

        /** @brief Returns the configured minimum bounds. */
        glm::vec3 GetMinBounds() const { return m_MinBounds; }

        /** @brief Returns the configured maximum bounds. */
        glm::vec3 GetMaxBounds() const { return m_MaxBounds; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const glm::vec3 min = context.RequireFloatVec<3>(object, "min");
            const glm::vec3 max = context.RequireFloatVec<3>(object, "max");
            auto module = CreateScope<SetPositionBox>(min, max);
            return module;
        }

    private:
        glm::vec3 m_MinBounds;
        glm::vec3 m_MaxBounds;
    };
}
