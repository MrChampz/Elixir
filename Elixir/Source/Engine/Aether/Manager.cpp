#include "epch.h"
#include "Manager.h"

#include <Engine/Aether/Effect.h>
#include <Engine/Aether/Renderer.h>
#include <Engine/Material/MaterialResolver.h>
#include <Engine/Material/MaterialSystem.h>

namespace Elixir::Aether
{
    Manager::Manager(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        MaterialRegistry& materialRegistry,
        MaterialSystem& materialSystem
    ) : m_EffectMaterials(materialRegistry),
        m_MaterialSystem(materialSystem),
        m_Renderer(CreateScope<Renderer>(context, shaderLoader, materialSystem)) {}

    Manager::~Manager() = default;

    Ref<System> Manager::LoadEffect(const std::filesystem::path& filepath) const
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
        const auto id = instance->GetId();

        const auto [_, inserted] = m_Instances.emplace(id, instance);
        EE_CORE_ASSERT(inserted, "Aether system instance UUID must be unique.")

        return instance;
    }

    bool Manager::DestroyInstance(const Ref<SystemInstance>& instance)
    {
        if (!IsManagedInstance(instance)) return false;

        const auto found = m_Instances.find(instance->GetId());
        if (found == m_Instances.end()) return false;

        m_FrameSubmissionPublisher.Remove(instance->GetId());
        GetRenderer().Retire(*instance);
        m_Instances.erase(found);

        return true;
    }

    void Manager::BeginFrame(const Timestep& timestep)
    {
        GetRenderer().Update(timestep);
    }

    Ref<FrameSubmission> Manager::CreateFrameSubmission() const
    {
        return CreateRef<FrameSubmission>();
    }

    bool Manager::Submit(FrameSubmission& submission, const Ref<SystemInstance>& instance) const
    {
        return IsManagedInstance(instance) && submission.Submit(*instance);
    }

    void Manager::PublishFrameSubmission(Ref<FrameSubmission> submission)
    {
        m_FrameSubmissionPublisher.Publish(std::move(submission));
    }

    void Manager::Render(const Camera& camera)
    {
        const auto submission = m_FrameSubmissionPublisher.Acquire();
        if (!submission) return;

        GetRenderer().Render(*submission, camera);
    }

    const SParticleSubmissionMetrics& Manager::GetLastSubmissionMetrics() const
    {
        return GetRenderer().GetLastSubmissionMetrics();
    }

    Renderer& Manager::GetRenderer() const
    {
        EE_CORE_ASSERT(m_Renderer, "Aether renderer is unavailable.")
        return *m_Renderer;
    }

    bool Manager::IsManagedInstance(const Ref<SystemInstance>& instance) const
    {
        if (!instance) return false;
        const auto found = m_Instances.find(instance->GetId());
        return found != m_Instances.end() && found->second.get() == instance.get();
    }
}
