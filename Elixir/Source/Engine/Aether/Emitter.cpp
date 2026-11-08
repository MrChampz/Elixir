#include "epch.h"
#include "Emitter.h"

namespace Elixir::Aether
{
    Emitter::Emitter(
        std::string name,
        const uint32_t maxParticles,
        const float spawnRate
    ) : m_Name(std::move(name)),
        m_MaxParticles(maxParticles),
        m_SpawnRate(spawnRate) {}

    bool Emitter::SetSolverConfig(const SSolverConfig& config)
    {
        if (!config.IsValid())
        {
            EE_CORE_ERROR("Invalid constraint solver configuration for emitter '{}'.", m_Name)
            return false;
        }

        m_Solver = config;
        return true;
    }

    bool Emitter::SetStrandRenderConfig(const SStrandRenderConfig& config)
    {
        if (!config.IsValid())
        {
            EE_CORE_ERROR("Invalid strand render configuration for emitter '{}'.", m_Name);
            return false;
        }

        m_StrandRender = config;
        return true;
    }

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

    std::optional<SCompiledEmitter> Emitter::Compile(
        const ParameterStore& paramStore,
        const std::vector<SCompiledParameter>& params,
        SCompiledModuleData& program
    ) const
    {
        SCompiledEmitter emitter;
        emitter.Id = m_Id;
        emitter.RenderMode = m_RenderMode;
        emitter.Solver = m_Solver;
        if (m_RenderMode == EParticleRenderMode::Strand)
            emitter.RenderConfig = CreateRef<Rendering::SStrandRenderConfig>(m_StrandRender);
        emitter.KillOnLifetimeExpired = m_KillOnLifetimeExpired;
        emitter.SimulationSpace = m_SimulationSpace;
        emitter.MaxParticles = m_MaxParticles;
        emitter.GravityScale = paramStore.GetFloat("GravityScale", 1.0f);
        emitter.SpawnOperationOffset = (uint32_t)program.Operations.size();
        emitter.SpawnRatePerSecond = m_SpawnRate;
        emitter.TriggerDelaySeconds = m_TriggerDelaySeconds;
        emitter.BurstCount = m_BurstCount;
        emitter.BurstIntervalSeconds = m_BurstIntervalSeconds;

        const uint32_t spawnRateParamIndex = FindScopedParameterIndex(
            params,
            m_Name,
            m_SpawnRateParamName
        );

        if (spawnRateParamIndex != UINT32_MAX)
        {
            if (params[spawnRateParamIndex].Type == EParameterType::UInt)
            {
                EE_CORE_ERROR("Aether spawn rate requires a floating-point parameter.");
            }
            else
            {
                emitter.SpawnRatePerSecond = params[spawnRateParamIndex].Value.x;
            }
        }

        if (m_Material)
            emitter.Material = m_Material;

        ModuleCompileContext context(program, params, m_Name, emitter.GravityScale);

        for (const auto& module : m_Modules)
        {
            if (module->GetPhase() != EModulePhase::Spawn)
                continue;

            module->Compile(context);
        }

        emitter.SpawnOperationCount = (uint32_t)program.Operations.size() - emitter.SpawnOperationOffset;
        emitter.UpdateOperationOffset = (uint32_t)program.Operations.size();
        emitter.ConstraintOffset = uint32_t(program.Constraints.size());

        for (const auto& module : m_Modules)
        {
            if (module->GetPhase() != EModulePhase::Update)
                continue;

            module->Compile(context);
        }

        emitter.UpdateOperationCount = (uint32_t)program.Operations.size() - emitter.UpdateOperationOffset;
        emitter.ConstraintCount =
            uint32_t(program.Constraints.size()) - emitter.ConstraintOffset;
        if (!context.IsValid())
            return std::nullopt;

        return emitter;
    }

}
