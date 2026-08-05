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

    Scope<SystemInstance> Manager::CreateInstance(Ref<const SCompiledSystem> system) const
    {
        if (!system) return nullptr;
        return CreateScope<SystemInstance>(std::move(system));
    }

    void Manager::BeginFrame(const Timestep& timestep)
    {
        // Release the previous frame's non-owning references before accepting
        // a new immutable submission.
        m_FrameSubmission.Reset();
        GetRenderer().Update(timestep);
    }

    bool Manager::Submit(const SystemInstance& instance)
    {
        return m_FrameSubmission.Submit(instance);
    }

    void Manager::Render(const Camera& camera)
    {
        GetRenderer().Render(m_FrameSubmission, camera);
    }

    void Manager::Retire(const SystemInstance& instance)
    {
        GetRenderer().Retire(instance);
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
