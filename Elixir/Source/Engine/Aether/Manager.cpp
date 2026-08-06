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

    SSystemInstanceHandle Manager::CreateInstance(Ref<const SCompiledSystem> system)
    {
        EE_CORE_ASSERT(system, "Aether system instance requires a compiled system.")

        auto instance = CreateScope<SystemInstance>(std::move(system));
        const auto id = instance->GetId();

        const auto [_, inserted] = m_Instances.emplace(id, std::move(instance));
        EE_CORE_ASSERT(inserted, "Aether system instance UUID must be unique.")

        return { id };
    }

    SystemInstance* Manager::FindInstance(const SSystemInstanceHandle& handle)
    {
        const auto found = m_Instances.find(handle.Id);
        return found != m_Instances.end() ? found->second.get() : nullptr;
    }

    const SystemInstance* Manager::FindInstance(const SSystemInstanceHandle& handle) const
    {
        const auto found = m_Instances.find(handle.Id);
        return found != m_Instances.end() ? found->second.get() : nullptr;
    }

    bool Manager::DestroyInstance(const SSystemInstanceHandle& handle)
    {
        const auto found = m_Instances.find(handle.Id);
        if (found == m_Instances.end()) return false;

        const auto& instance = *found->second;
        m_FrameSubmission.Remove(instance);
        GetRenderer().Retire(instance);
        m_Instances.erase(found);

        return true;
    }

    void Manager::BeginFrame(const Timestep& timestep)
    {
        // Release the previous frame's non-owning references before accepting
        // a new immutable submission.
        m_FrameSubmission.Reset();
        GetRenderer().Update(timestep);
    }

    bool Manager::Submit(const SSystemInstanceHandle& handle)
    {
        const auto* instance = FindInstance(handle);
        if (!instance) return false;

        return m_FrameSubmission.Submit(*instance);
    }

    void Manager::Render(const Camera& camera)
    {
        GetRenderer().Render(m_FrameSubmission, camera);
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
}
