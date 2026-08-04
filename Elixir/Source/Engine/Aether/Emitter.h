#pragma once

#include <Engine/Core/UUID.h>
#include <Engine/Aether/Modules.h>
#include <Engine/Aether/ParameterStore.h>
#include <Engine/Aether/CurveStore.h>
#include <Engine/Aether/ColorCurveStore.h>
#include <Engine/Aether/ParticleMaterialDefinition.h>
#include <Engine/Aether/ParticleMaterialFactory.h>
#include <Engine/Material/MaterialInstance.h>
#include <Engine/Material/MaterialResolver.h>

namespace Elixir::Aether
{
    class ParameterStore;

    struct SCompiledEmitter
    {
        UUID Id;
        std::string Name;
        EParticleRenderMode RenderMode = EParticleRenderMode::Sprite;
        EParticleSimulationSpace SimulationSpace = EParticleSimulationSpace::World;

        // Immutable GPU material state published by System::Compile.
        Ref<const MaterialRenderProxy> Material;

        float SpawnRatePerSecond = 1.0f;
        uint32_t BurstCount = 0u;
        float BurstIntervalSeconds = 0.0f;
        int32_t TriggerSourceEmitterIndex = -1;
        float TriggerDelaySeconds = 0.0f;
        uint32_t TriggerTargetOffset = 0;
        uint32_t TriggerTargetCount = 0;
        bool IsTriggerDriven = false;

        float GravityScale = 1.0f;

        // Relative to the beginning of this compiled system. It is not a
        // physical address in a shared GPU particle pool.
        uint32_t LocalParticleOffset = 0;

        uint32_t MaxParticles = 0u;
        uint32_t SpawnOpOffset = 0u;
        uint32_t SpawnOpCount = 0u;
        uint32_t UpdateOpOffset = 0u;
        uint32_t UpdateOpCount = 0u;

        bool operator==(const SCompiledEmitter& other) const noexcept
        {
            return Id == other.Id;
        }

        auto GetHashParams() const
        {
            return Id;
        }
    };
}

GENERATE_HASH_FUNCTION(Elixir::Aether::SCompiledEmitter)

namespace Elixir::Aether
{
    class ELIXIR_API Emitter final
    {
      public:
        /**
         * Create a new Emitter.
         * @param name Emitter name.
         * @param maxParticles Max particles in the emitter.
         * @param spawnRate Spawn rate per second.
         */
        Emitter(const std::string& name, uint32_t maxParticles, float spawnRate);

        Emitter(Emitter&&) = default;
        Emitter& operator=(Emitter&&) = default;

        Emitter(const Emitter&) = delete;
        Emitter& operator=(const Emitter&) = delete;

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

        EParticleRenderMode GetRenderMode() const { return m_RenderMode; }
        void SetRenderMode(const EParticleRenderMode mode) { m_RenderMode = mode; }

        EParticleSimulationSpace GetSimulationSpace() const { return m_SimulationSpace; }
        void SetSimulationSpace(const EParticleSimulationSpace space) { m_SimulationSpace = space; }

        void SetBurst(uint32_t count, float intervalSeconds);

        void SetTriggerEmitter(std::string emitterName, float delaySeconds);

        SCompiledEmitter Compile(
            const ParameterStore& paramStore,
            const std::vector<SGPUParameter>& params,
            std::vector<SGPUParticleOp>& ops,
            MaterialResolver& materialResolver
        ) const;

        const std::string& GetName() const { return m_Name; }
        uint32_t GetMaxParticles() const { return m_MaxParticles; }

        const std::optional<SParticleMaterialDefinition>& GetMaterialDefinition() const
        {
            return m_MaterialDefinition;
        }

        void SetMaterialDefinition(SParticleMaterialDefinition definition)
        {
            m_MaterialDefinition = std::move(definition);
        }

        const Ref<MaterialInstance>& GetMaterial() const { return m_Material; }
        void SetMaterial(const Ref<Material>& material);
        void SetMaterial(Ref<MaterialInstance> material) { m_Material = std::move(material); }

        uint32_t GetBurstCount() const { return m_BurstCount; }
        float GetBurstIntervalSeconds() const { return m_BurstIntervalSeconds; }

        const std::string& GetTriggerEmitterName() const { return m_TriggerEmitterName; }
        float GetTriggerDelaySeconds() const { return m_TriggerDelaySeconds; }

        ParameterStore& GetParameters() { return m_Parameters; }
        const ParameterStore& GetParameters() const { return m_Parameters; }
        CurveStore& GetCurves() { return m_Curves; }
        const CurveStore& GetCurves() const { return m_Curves; }
        ColorCurveStore& GetColorCurves() { return m_ColorCurves; }
        const ColorCurveStore& GetColorCurves() const { return m_ColorCurves; }

        const std::string& GetSpawnRateParamName() const { return m_SpawnRateParamName; }
        void SetSpawnRateParamName(const std::string& paramName) { m_SpawnRateParamName = paramName; }

      private:
        UUID m_Id;
        std::string m_Name;
        EParticleRenderMode m_RenderMode = EParticleRenderMode::Sprite;
        EParticleSimulationSpace m_SimulationSpace = EParticleSimulationSpace::World;
        std::optional<SParticleMaterialDefinition> m_MaterialDefinition;
        Ref<MaterialInstance> m_Material;
        uint32_t m_MaxParticles;

        std::vector<Scope<ParticleSpawnModule>> m_SpawnModules;
        std::vector<Scope<ParticleUpdateModule>> m_UpdateModules;

        std::string m_SpawnRateParamName;
        float m_SpawnRate = 0.0f; // per second
        uint32_t m_BurstCount = 0u;
        float m_BurstIntervalSeconds = 0.0f;
        std::string m_TriggerEmitterName;
        float m_TriggerDelaySeconds = 0.0f;

        ParameterStore m_Parameters;
        CurveStore m_Curves;
        ColorCurveStore m_ColorCurves;
    };
}
