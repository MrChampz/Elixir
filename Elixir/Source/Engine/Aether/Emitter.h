#pragma once

#include "ParameterStore.h"

#include <Engine/Core/Timer.h>
#include <Engine/Aether/Particle.h>
#include <Engine/Aether/Modules.h>

namespace Elixir::Aether
{
    struct SSpawnContext;
    struct SRenderParticle;
    class ParameterStore;

    struct SGPUEmitter
    {
        glm::vec2 SpawnCenter{};
        float SpawnRadius = 0.01f;
        float SpawnRatePerSecond = 1.0f;
        float AngleMinRadians = 0.0f;
        float AngleMaxRadians = 0.0f;
        float SpeedMin = 0.0f;
        float SpeedMax = 0.0f;
        float LifetimeMin = 1.0f;
        float LifetimeMax = 1.0f;
        float SizeStart = 8.0f;
        float SizeEnd = 1.0f;
        glm::vec4 ColorStart{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 ColorEnd{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec2 Gravity{};
        float GravityScale = 1.0f;
        float Drag = 0.0f;
        glm::vec2 MinBounds{ -1.0f, -1.0f };
        glm::vec2 MaxBounds{ 1.0f, 1.0f };
        uint32_t MaxParticles = 0;
    };

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

        SGPUEmitter Build(const ParameterStore& params) const;

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