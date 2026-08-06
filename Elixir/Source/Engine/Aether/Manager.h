#pragma once

#include <Engine/Aether/EffectMaterialResolver.h>
#include <Engine/Aether/FrameSubmission.h>
#include <Engine/Aether/SystemInstance.h>
#include <Engine/Aether/SystemInstanceHandle.h>

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

        SSystemInstanceHandle CreateInstance(Ref<const SCompiledSystem> system);

        // The returned pointer is a temporary borrow. Do not store it across
        // DestroyInstance() or use it from another thread.
        SystemInstance* FindInstance(const SSystemInstanceHandle& handle);
        const SystemInstance* FindInstance(const SSystemInstanceHandle& handle) const;

        // Must be called from the render-frame callback. It detaches any
        // frame submission before the renderer retires GPU allocations.
        bool DestroyInstance(const SSystemInstanceHandle& handle);

        // Render-frame API. Submit() is valid only between BeginFrame() and Render().
        void BeginFrame(const Timestep& timestep);

        bool Submit(const SSystemInstanceHandle& handle);

        void Render(const Camera& camera);

        const SParticleSubmissionMetrics& GetLastSubmissionMetrics() const;

    private:
        Renderer& GetRenderer() const;

        EffectMaterialResolver m_EffectMaterials;

        MaterialSystem& m_MaterialSystem;
        std::unordered_map<UUID, Scope<SystemInstance>> m_Instances;

        FrameSubmission m_FrameSubmission;
        Scope<Renderer> m_Renderer;
    };
}