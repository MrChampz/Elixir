#pragma once

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Sets spawn positions along a path with two circular amplitudes. */
    class ELIXIR_API SetPositionCircularPath final : public SpawnModule
    {
    public:
        /** @brief Creates the module with the supplied values. */
        explicit SetPositionCircularPath(
            glm::vec3 baseOffset,
            glm::vec3 primaryAmplitude,
            glm::vec3 secondaryAmplitude,
            float timeScale
        ) : m_BaseOffset(baseOffset),
            m_PrimaryAmplitude(primaryAmplitude),
            m_SecondaryAmplitude(secondaryAmplitude),
            m_TimeScale(timeScale) {}

        /** @brief Appends this module's GPU operations to the supplied context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit(Core::EParticleOp::SetPositionCircularPath, Core::EParticleAttribute::Position, {
                context.Vector(m_BaseOffset),
                context.Scalar(m_TimeScale),
                context.Vector(m_PrimaryAmplitude),
                context.Vector(m_SecondaryAmplitude)
            });
        }

        /** @brief Returns the configured base offset. */
        glm::vec3 GetBaseOffset() const { return m_BaseOffset; }

        /** @brief Returns the configured primary amplitude. */
        glm::vec3 GetPrimaryAmplitude() const { return m_PrimaryAmplitude; }

        /** @brief Returns the configured secondary amplitude. */
        glm::vec3 GetSecondaryAmplitude() const { return m_SecondaryAmplitude; }

        /** @brief Returns the configured time scale. */
        float GetTimeScale() const { return m_TimeScale; }

        /** @brief Creates this module from its serialized fields. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const glm::vec3 baseOffset = context.RequireFloatVec<3>(object, "baseOffset");
            const glm::vec3 primary = context.RequireFloatVec<3>(object, "primaryAmplitude");
            const glm::vec3 secondary = context.RequireFloatVec<3>(object, "secondaryAmplitude");
            const float timeScale = context.RequireFloat(object, "timeScale");
            auto module = CreateScope<SetPositionCircularPath>(baseOffset, primary, secondary, timeScale);
            return module;
        }

    private:
        glm::vec3 m_BaseOffset;
        glm::vec3 m_PrimaryAmplitude;
        glm::vec3 m_SecondaryAmplitude;
        float m_TimeScale;
    };
}
