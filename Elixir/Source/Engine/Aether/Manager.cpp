#include "epch.h"
#include "Manager.h"

#include <Engine/Materials/MaterialSystem.h>
#include <Engine/Aether/Effect/Effect.h>
#include <Engine/Aether/Runtime/InstanceRegistry.h>
#include <Engine/Aether/Simulation/Simulator.h>
#include <Engine/Aether/Rendering/Renderer.h>

namespace Elixir::Aether
{
    using namespace Effect;

    Manager::Manager(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        MaterialRegistry& materialRegistry,
        MaterialSystem& materialSystem
    ) : m_Runtime(CreateScope<InstanceRegistry>(
            materialRegistry,
            materialSystem
        )),
        m_Simulator(CreateScope<Simulator>(context, shaderLoader)),
        m_Renderer(CreateScope<Renderer>(context)),
        m_MaterialSystem(materialSystem),
        m_GraphicsContext(context) {}

    Manager::~Manager() = default;

    Ref<System> Manager::LoadEffect(const std::filesystem::path& filepath)
    {
        return LoadEffectFile(filepath);
    }

    bool Manager::Recompile(const Ref<System>& system)
    {
        return GetRuntime().Recompile(system);
    }

    bool Manager::Add(const Ref<SystemInstance>& instance)
    {
        return GetRuntime().Register(instance);
    }

    bool Manager::Remove(const Ref<SystemInstance>& instance)
    {
        const auto detached = GetRuntime().Unregister(instance);
        if (!detached) return false;

        m_PendingRetirements.Enqueue(detached);

        return true;
    }

    void Manager::BeginFrame(const Timestep& timestep)
    {
        GetRuntime().PublishActiveInstances();
        GetSimulator().BeginFrame(timestep);
        RetireDestroyedInstances();
    }

    void Manager::Render(const Camera& camera)
    {
        const auto submission = GetRuntime().AcquireSubmission();
        if (!submission) return;

        const auto cmd = m_GraphicsContext->GetSecondaryCommandBuffer();

        cmd->Begin({
            .ColorAttachment = m_GraphicsContext->GetRenderTarget(),
            .DepthStencilAttachment =  m_GraphicsContext->GetDepthStencilRenderTarget(),
            .RenderArea = m_GraphicsContext->GetRenderTarget()->GetExtent(),
        });

        const auto frame = GetSimulator().Simulate(*submission, cmd);
        cmd->End();
        m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);

        m_MaterialSystem.Submit(GetRenderer().BuildMaterialRenderScene(*frame, camera));
    }

    const SSimulationMetrics& Manager::GetLastSimulationMetrics() const
    {
        return GetSimulator().GetLastMetrics();
    }

    const SRenderingMetrics& Manager::GetLastRenderingMetrics() const
    {
        return GetRenderer().GetLastMetrics();
    }

    InstanceRegistry& Manager::GetRuntime() const
    {
        EE_CORE_ASSERT(m_Runtime, "Aether runtime is unavailable.")
        return *m_Runtime;
    }

    Simulator& Manager::GetSimulator() const
    {
        EE_CORE_ASSERT(m_Simulator, "Aether simulator is unavailable.")
        return *m_Simulator;
    }

    Renderer& Manager::GetRenderer() const
    {
        EE_CORE_ASSERT(m_Renderer, "Aether renderer is unavailable.")
        return *m_Renderer;
    }

    void Manager::RetireDestroyedInstances()
    {
        const auto instances = m_PendingRetirements.Drain();

        for (const auto& instance : instances)
            GetSimulator().Retire(instance->GetKey());
    }
}
