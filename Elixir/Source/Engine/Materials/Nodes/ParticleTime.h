#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/Compilation/MaterialEmitContext.h>

namespace Elixir::Materials::Nodes
{
    /** Selects a time value available in every particle geometry mode. */
    enum class EParticleTime : uint8_t
    {
        Age,
        Lifetime,
        NormalizedAge
    };

    /** @brief Reads particle timing without depending on a particular effect or emitter. */
    class ParticleTime final : public MaterialNode
    {
    public:
        /**
         * @brief Selects an elapsed, total, or normalized particle lifetime.
         * @param value Time value supplied by the particle renderer.
         */
        explicit ParticleTime(const EParticleTime value = EParticleTime::NormalizedAge)
          : m_Value(value) {}

        /** @copydoc MaterialNode::GetTypeName */
        std::string_view GetTypeName() const override { return "Material.ParticleTime"; }

        /** @copydoc MaterialNode::GetSupportedStages */
        EShaderStage GetSupportedStages(const SMaterialGraphBindings* bindings) const override
        {
            return EShaderStage::Pixel;
        }

        /** @copydoc MaterialNode::Emit */
        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto age = context.ParticleAge();
            const auto lifetime = context.ParticleLifetime();

            if (m_Value == EParticleTime::Age)
                return { age, EMaterialValueType::Float };

            if (m_Value == EParticleTime::Lifetime)
                return { lifetime, EMaterialValueType::Float };

            return {
                "saturate((" + age + ") / max((" + lifetime + "), 0.001))",
                EMaterialValueType::Float
            };
        }

    private:
        EParticleTime m_Value;
    };
}
