#include "epch.h"
#include "Manager.h"

#include <Engine/Material/MaterialResolver.h>
#include <Engine/Material/MaterialSystem.h>
#include <Engine/Aether/Effect/Effect.h>
#include <Engine/Aether/Simulation/Simulator.h>
#include <Engine/Aether/Rendering/Renderer.h>

using namespace Elixir::Aether::Effect;

namespace Elixir::Aether
{
    Manager::Manager(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        MaterialRegistry& materialRegistry,
        MaterialSystem& materialSystem
    ) : m_EffectMaterials(materialRegistry),
        m_MaterialSystem(materialSystem),
        m_Simulator(CreateScope<Simulator>(context, shaderLoader)),
        m_Renderer(CreateScope<Renderer>(context, materialSystem)),
        m_GraphicsContext(context) {}

    Manager::~Manager() = default;

    Ref<System> Manager::LoadEffect(const std::filesystem::path& filepath)
    {
        return LoadEffectFile(filepath);
    }

    Ref<const SCompiledSystem> Manager::Compile(System& system) const
    {
        if (!m_EffectMaterials.Resolve(system))
        {
            EE_CORE_ERROR("Could not resolve materials for Aether system '{}'.", system.GetName())
            return nullptr;
        }

        return CreateRef<SCompiledSystem>(system.Compile(m_MaterialSystem));
    }

    Ref<SystemInstance> Manager::CreateInstance(Ref<const SCompiledSystem> system)
    {
        EE_CORE_ASSERT(system, "Aether system instance requires a compiled system.")

        auto instance = CreateRef<SystemInstance>(std::move(system));
        const auto key = instance->GetKey();
        const std::scoped_lock lock(m_InstancesMutex);

        const auto [_, inserted] = m_Instances.emplace(key, instance);
        EE_CORE_ASSERT(inserted, "Aether system instance UUID must be unique.")

        return instance;
    }

    bool Manager::DestroyInstance(const Ref<SystemInstance>& instance)
    {
        const std::scoped_lock lock(m_InstancesMutex);
        if (!IsManagedInstance(instance)) return false;

        const auto found = m_Instances.find(instance->GetKey());
        if (found == m_Instances.end()) return false;

        m_FrameSubmissionPublisher.Remove(*instance);
        m_PendingRetirements.Enqueue(found->second);
        m_Instances.erase(found);

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
        const std::scoped_lock lock(m_InstancesMutex);
        return IsManagedInstance(instance) && submission.Submit(*instance);
    }

    void Manager::PublishFrameSubmission(Ref<FrameSubmission> submission)
    {
        EE_CORE_ASSERT(submission, "Aether frame submission cannot be null.")

        // Keep the registry lock until publishing is complete. DestroyInstance()
        // takes the same lock before removing a published frame.
        const std::lock_guard lock(m_InstancesMutex);
        m_FrameSubmissionPublisher.Publish(
            std::move(submission),
            [this](const SSystemInstanceKey& key)
            {
                return m_Instances.contains(key);
            }
        );
    }

    void Manager::Render(const Camera& camera)
    {
        const auto submission = m_FrameSubmissionPublisher.Acquire();
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

    bool Manager::IsManagedInstance(const Ref<SystemInstance>& instance) const
    {
        if (!instance) return false;
        const auto found = m_Instances.find(instance->GetKey());
        return found != m_Instances.end() && found->second.get() == instance.get();
    }
}
