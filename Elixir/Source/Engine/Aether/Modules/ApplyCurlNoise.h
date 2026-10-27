#pragma once

#include <Engine/Aether/Modules/Module.h>
#include <Engine/Aether/Effect/ModuleParseContext.h>

namespace Elixir::Aether::Modules
{
    /** @brief Adds an animated curl-noise force to particle velocity or free strand points. */
    class ELIXIR_API ApplyCurlNoise final : public UpdateModule
    {
    public:
        /** @brief Creates a curl force; frequency uses inverse simulation-space units and scroll uses field units per second. */
        explicit ApplyCurlNoise(float strength = 0.65f, float frequency = 2.2f,
            glm::vec3 scrollVelocity = {0.12f, 0.09f, 0.07f}, uint32_t seed = 0)
            : m_Strength(strength), m_Frequency(frequency), m_ScrollVelocity(scrollVelocity), m_Seed(seed) {}

        /** @brief Supports particle velocity and free strand points. */
        bool SupportsTarget(EModuleTarget target) const override
        {
            return target == EModuleTarget::Particle || target == EModuleTarget::StrandPoints;
        }

        /** @brief Appends the curl-force instruction with optional numeric parameter bindings. */
        void Compile(ModuleCompileContext& context) const override
        {
            if (!std::isfinite(m_Strength) || m_Strength < 0 || !std::isfinite(m_Frequency) || m_Frequency <= 0 ||
                !std::isfinite(m_ScrollVelocity.x) || !std::isfinite(m_ScrollVelocity.y) || !std::isfinite(m_ScrollVelocity.z))
            {
                EE_CORE_ERROR("Invalid curl noise force settings.");
                return;
            }
            context.Emit(EOperationType::ApplyCurlNoise, EParticleAttribute::Velocity,
                {context.Scalar(m_Strength, m_StrengthParameter), context.Scalar(m_Frequency, m_FrequencyParameter),
                 context.Vector(m_ScrollVelocity, m_ScrollParameter), context.UInt(m_Seed, m_SeedParameter)});
        }

        /** @brief Binds runtime strength, frequency, scroll, and seed parameters. */
        ApplyCurlNoise& BindParameters(std::string strength, std::string frequency, std::string scroll, std::string seed)
        {
            m_StrengthParameter = std::move(strength);
            m_FrequencyParameter = std::move(frequency);
            m_ScrollParameter = std::move(scroll);
            m_SeedParameter = std::move(seed);
            return *this;
        }

        /** @brief Reads literals or parameter references for an authored curl force. */
        static Scope<Module> Create(Effect::ModuleParseContext& context, simdjson::ondemand::object& object)
        {
            const auto strength = context.ParseScalar(object, "strength", 0.65f);
            const auto frequency = context.ParseScalar(object, "frequency", 2.2f);
            const auto scroll = context.ParseFloat4(object, "scrollVelocity", glm::vec4(0.12f, 0.09f, 0.07f, 0), true);
            uint32_t seed = 0;
            std::string seedParameter;
            if (context.HasField(object, "seed"))
            {
                // Integer literals preserve all seed bits; scalar bindings convert at runtime.
                auto field = object["seed"];
                std::string_view value;
                if (!field.get_string().get(value))
                {
                    if (value.size() < 2 || value.front() != '$') context.Fail("Curl seed must be an unsigned integer or parameter reference.");
                    else seedParameter = std::string(value.substr(1));
                }
                else seed = context.RequireUInt(object, "seed");
            }
            if (strength.Value < 0 || frequency.Value <= 0) context.Fail("Curl strength must be nonnegative and frequency must be positive.");
            auto module = CreateScope<ApplyCurlNoise>(strength.Value, frequency.Value, glm::vec3(scroll.Value), seed);
            module->BindParameters(strength.Param, frequency.Param, scroll.Param, std::move(seedParameter));
            return module;
        }

    private:
        float m_Strength;
        float m_Frequency;
        glm::vec3 m_ScrollVelocity;
        uint32_t m_Seed;
        std::string m_StrengthParameter, m_FrequencyParameter, m_ScrollParameter, m_SeedParameter;
    };
}
