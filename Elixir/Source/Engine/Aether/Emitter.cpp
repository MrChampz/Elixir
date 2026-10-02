#include "epch.h"
#include "Emitter.h"

namespace Elixir::Aether
{
    Emitter::Emitter(
        const std::string& name,
        const uint32_t maxParticles,
        const float spawnRate
    ) : m_Name(name),
        m_MaxParticles(maxParticles),
        m_SpawnRate(spawnRate) {}

    void Emitter::SetMaterial(const Ref<Material>& material)
    {
        if (!material)
        {
            EE_CORE_ERROR("Trying to set a null material to emitter.")
            return;
        }

        SetMaterial(material->CreateInstance());
    }

    void Emitter::SetBurst(const uint32_t count, const float intervalSeconds)
    {
        m_BurstCount = count;
        m_BurstIntervalSeconds = intervalSeconds;
    }

    void Emitter::SetTriggerEmitter(std::string emitterName, const float delaySeconds)
    {
        m_TriggerEmitterName = std::move(emitterName);
        m_TriggerDelaySeconds = delaySeconds;
    }

    SCompiledEmitter Emitter::Compile(
        const ParameterStore& paramStore,
        const std::vector<SGPUParameter>& params,
        std::vector<SGPUParticleOp>& ops
    ) const
    {
        SCompiledEmitter emitter;
        emitter.Id = m_Id;
        emitter.RenderMode = m_RenderMode;
        emitter.SimulationSpace = m_SimulationSpace;
        emitter.MaxParticles = m_MaxParticles;
        emitter.GravityScale = paramStore.GetFloat("GravityScale", 1.0f);
        emitter.SpawnOpOffset = (uint32_t)ops.size();
        emitter.SpawnRatePerSecond = m_SpawnRate;
        emitter.TriggerDelaySeconds = m_TriggerDelaySeconds;
        emitter.BurstCount = m_BurstCount;
        emitter.BurstIntervalSeconds = m_BurstIntervalSeconds;

        const uint32_t spawnRateParamIndex =
            FindScopedParameterIndex(params, m_Name, m_SpawnRateParamName);
        if (spawnRateParamIndex != UINT32_MAX)
            emitter.SpawnRatePerSecond = params[spawnRateParamIndex].Value.x;

        if (m_Material)
            emitter.Material = m_Material;

        ModuleCompileContext context(ops, params, m_Name, emitter.GravityScale);

        for (const auto& module : m_Modules)
        {
            if (module->GetPhase() != EModulePhase::Spawn)
                continue;

            module->Compile(context);
        }

        emitter.SpawnOpCount = (uint32_t)ops.size() - emitter.SpawnOpOffset;
        emitter.UpdateOpOffset = (uint32_t)ops.size();

        for (const auto& module : m_Modules)
        {
            if (module->GetPhase() != EModulePhase::Update)
                continue;

            module->Compile(context);
        }

        emitter.UpdateOpCount = (uint32_t)ops.size() - emitter.UpdateOpOffset;

        return emitter;
    }
}
