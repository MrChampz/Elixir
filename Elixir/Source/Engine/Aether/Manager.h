#pragma once

#include <Engine/Aether/EffectMaterialResolver.h>
#include <Engine/Aether/FrameSubmission.h>
#include <Engine/Aether/SystemInstance.h>

namespace Elixir
{
    class Camera;
    class GraphicsContext;
    class ShaderLoader;
    class Timestep;

    class MaterialRegistry;
    class MaterialResolver;
    class MaterialSystem;
}

namespace Elixir::Aether
{
    class Renderer;
    struct SParticleSubmissionMetrics;

    // Application-scoped entry point for effect assets and their immutable
    // runtime payloads. It owns the particle renderer, while the renderer
    // retains GPU allocation, synchronization, and draw implementation.
    class ELIXIR_API Manager final
    {
    public:
        Manager(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            MaterialRegistry& materialRegistry,
            MaterialSystem& materialSystem
        );

        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;
        Manager(Manager&&) = delete;
        Manager& operator=(Manager&&) = delete;

        Ref<System> LoadEffect(const std::filesystem::path& filepath) const;

        Ref<const SCompiledSystem> Compile(System& system) const;

        // The returned runtime instance is configured through its public API.
        // Manager retains registration and GPU lifetime ownership.
        Ref<SystemInstance> CreateInstance(Ref<const SCompiledSystem> system);

        // Must be called from the render-frame callback. It detaches any
        // frame submission before the renderer retires GPU allocations.
        bool DestroyInstance(const Ref<SystemInstance>& instance);

        void BeginFrame(const Timestep& timestep);

        // A submission is built by one producer thread, then atomically
        // published for the renderer thread to consume.
        Ref<FrameSubmission> CreateFrameSubmission() const;

        bool Submit(FrameSubmission& submission, const Ref<SystemInstance>& instance) const;

        void PublishFrameSubmission(Ref<FrameSubmission> submission);

        void Render(const Camera& camera);

        const SParticleSubmissionMetrics& GetLastSubmissionMetrics() const;

    private:
        Renderer& GetRenderer() const;

        bool IsManagedInstance(const Ref<SystemInstance>& instance) const;

        EffectMaterialResolver m_EffectMaterials;

        MaterialSystem& m_MaterialSystem;
        std::unordered_map<UUID, Ref<SystemInstance>> m_Instances;

        FrameSubmissionPublisher m_FrameSubmissionPublisher;
        Scope<Renderer> m_Renderer;
    };
}