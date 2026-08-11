#include "epch.h"
#include "Manager.h"

#include <Engine/Material/MaterialSystem.h>
#include <Engine/Aether/Effect/Effect.h>
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
        m_Renderer(CreateScope<Renderer>(context, materialSystem)),
        m_GraphicsContext(context) {}

    Manager::~Manager() = default;

    Ref<System> Manager::LoadEffect(const std::filesystem::path& filepath)
    {
        return LoadEffectFile(filepath);
    }

    Ref<SystemInstance> Manager::CreateInstance(const Ref<System>& system)
    {
        return GetRuntime().CreateInstance(system);
    }

    bool Manager::Recompile(const Ref<System>& system)
    {
        return GetRuntime().Recompile(system);
    }

    bool Manager::DestroyInstance(const Ref<SystemInstance>& instance)
    {
        const auto detached = GetRuntime().DetachInstance(instance);
        if (!detached) return false;

        m_PendingRetirements.Enqueue(detached);

        return true;
    }

    void Manager::BeginFrame(const Timestep& timestep)
    {
        GetSimulator().BeginFrame(timestep);
        RetireDestroyedInstances();
    }

    Ref<FrameSubmission> Manager::CreateFrameSubmission()
    {
        return CreateRef<FrameSubmission>();
    }

    bool Manager::Submit(
        FrameSubmission& submission,
        const Ref<SystemInstance>& instance
    ) const
    {
        return GetRuntime().Submit(submission, instance);
    }

    void Manager::PublishFrameSubmission(Ref<FrameSubmission> submission)
    {
        EE_CORE_ASSERT(submission, "Aether frame submission cannot be null.")
        GetRuntime().Publish(std::move(submission));
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
        GetRenderer().Render(*frame, camera, cmd);

        cmd->End();
        m_GraphicsContext->EnqueueSecondaryCommandBuffer(cmd);
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
