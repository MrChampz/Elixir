#pragma once

#include <Engine/Core/Timer.h>
#include <Engine/Aether/Particle.h>
#include <Engine/Aether/Modules.h>

namespace Elixir::Aether
{
    struct SSpawnContext;
    struct SRenderParticle;
    class ParameterStore;

    class ELIXIR_API Emitter final
    {
      public:
        Emitter(const std::string& name, uint32_t maxParticles, float spawnRate);

        void Update(const Timestep& timestep, const ParameterStore& params);

        template <typename Module, typename... Args>
        Module& AddSpawnModule(Args&&... args)
        {
            auto module = CreateScope<Module>(std::forward<Args>(args)...);
            auto& ref = *module;
            m_SpawnModules.push_back(std::move(module));
            return ref;
        }

        template <typename Module, typename... Args>
        Module& AddUpdateModule(Args&&... args)
        {
            auto module = CreateScope<Module>(std::forward<Args>(args)...);
            auto& ref = *module;
            m_UpdateModules.push_back(std::move(module));
            return ref;
        }

        const std::string& GetName() const { return m_Name; }
        void GatherRenderParticles(std::vector<SRenderParticle>& output) const;

      private:
        void SpawnParticle(SSpawnContext& context, const ParameterStore& params);

        std::string m_Name;
        std::vector<SParticle> m_Particles;
        std::vector<Scope<ParticleSpawnModule>> m_SpawnModules;
        std::vector<Scope<ParticleUpdateModule>> m_UpdateModules;
        float m_SpawnRate = 0.0f; // per seconds
        float m_SpawnAccumulator = 0.0f;
        std::mt19937 m_RNG;
    };
}