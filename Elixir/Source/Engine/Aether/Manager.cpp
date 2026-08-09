#include "epch.h"
#include "Manager.h"

#include <Engine/Material/MaterialResolver.h>
#include <Engine/Material/MaterialSystem.h>
#include <Engine/Aether/Effect/Effect.h>
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
        m_Renderer(CreateScope<Rendering::Renderer>(context, shaderLoader, materialSystem)) {}

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
        GetRenderer().Update(timestep);
        RetireDestroyedInstances();
    }

    Ref<Rendering::FrameSubmission> Manager::CreateFrameSubmission()
    {
        return CreateRef<Rendering::FrameSubmission>();
    }

    bool Manager::Submit(
        Rendering::FrameSubmission& submission,
        const Ref<SystemInstance>& instance
    ) const
    {
        const std::scoped_lock lock(m_InstancesMutex);
        return IsManagedInstance(instance) && submission.Submit(*instance);
    }

    void Manager::PublishFrameSubmission(Ref<Rendering::FrameSubmission> submission)
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

        GetRenderer().Render(*submission, camera);
    }

    const Rendering::SParticleSubmissionMetrics& Manager::GetLastSubmissionMetrics() const
    {
        return GetRenderer().GetLastSubmissionMetrics();
    }

    Rendering::Renderer& Manager::GetRenderer() const
    {
        EE_CORE_ASSERT(m_Renderer, "Aether renderer is unavailable.")
        return *m_Renderer;
    }

    void Manager::RetireDestroyedInstances()
    {
        const auto instances = m_PendingRetirements.Drain();

        for (const auto& instance : instances)
            GetRenderer().Retire(instance->GetKey());
    }

    bool Manager::IsManagedInstance(const Ref<SystemInstance>& instance) const
    {
        if (!instance) return false;
        const auto found = m_Instances.find(instance->GetKey());
        return found != m_Instances.end() && found->second.get() == instance.get();
    }
}
