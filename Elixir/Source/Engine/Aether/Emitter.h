#pragma once

#include "ParameterStore.h"

#include <Engine/Core/UUID.h>
#include <Engine/Core/Timer.h>
#include <Engine/Aether/Particle.h>
#include <Engine/Aether/Modules.h>

namespace Elixir::Aether
{
    struct SSpawnContext;
    struct SRenderParticle;
    class ParameterStore;

    enum class EParticleRenderMode : uint8_t
    {
        Sprite = 0,
        Ribbon
    };

    struct SGPUEmitter
    {
        UUID m_UUID;
        std::string Name;
        EParticleRenderMode RenderMode = EParticleRenderMode::Sprite;
        uint32_t ParticleOffset = 0;
        float SpawnRatePerSecond = 1.0f;
        float GravityScale = 1.0f;
        uint32_t MaxParticles = 0;
        uint32_t SpawnModuleOffset = 0;
        uint32_t SpawnModuleCount = 0;
        uint32_t UpdateModuleOffset = 0;
        uint32_t UpdateModuleCount = 0;

        bool operator==(const SGPUEmitter& other) const noexcept
        {
            return m_UUID == other.m_UUID;
        }

        auto GetHashParams() const
        {
            return m_UUID;
        }
    };
}

GENERATE_HASH_FUNCTION(Elixir::Aether::SGPUEmitter)

namespace Elixir::Aether
{
    class ELIXIR_API Emitter final
    {
      public:
        Emitter(const std::string& name, uint32_t maxParticles, float spawnRate);

        Emitter(Emitter&&) = default;
        Emitter& operator=(Emitter&&) = default;

        Emitter(const Emitter&) = delete;
        Emitter& operator=(const Emitter&) = delete;

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

        void SetRenderMode(EParticleRenderMode mode) { m_RenderMode = mode; }

        SGPUEmitter Build(const ParameterStore& params, std::vector<SGPUModule>& modules) const;

        const std::string& GetName() const { return m_Name; }
        void GatherRenderParticles(std::vector<SRenderParticle>& output) const;

      private:
        void SpawnParticle(SSpawnContext& context, const ParameterStore& params);

        UUID m_UUID;
        std::string m_Name;
        EParticleRenderMode m_RenderMode;
        std::vector<SParticle> m_Particles;
        std::vector<Scope<ParticleSpawnModule>> m_SpawnModules;
        std::vector<Scope<ParticleUpdateModule>> m_UpdateModules;
        float m_SpawnRate = 0.0f; // per seconds
        float m_SpawnAccumulator = 0.0f;
        std::mt19937 m_RNG;
    };
}
