#pragma once

#include <Engine/Aether/SystemInstance.h>
#include <Engine/Aether/Effect/MaterialResolver.h>
#include <Engine/Aether/Rendering/FrameSubmission.h>
#include <Engine/Aether/Rendering/SystemInstanceRetirementQueue.h>

namespace Elixir
{
    class Camera;
    class GraphicsContext;
    class ShaderLoader;
    class Timestep;

    class MaterialRegistry;
    class MaterialResolver;
    class MaterialSystem;

    namespace Aether::Rendering
    {
        class Renderer;
        struct SParticleSubmissionMetrics;
    }
}

namespace Elixir::Aether
{
    /**
     * @brief Coordinates Aether effect compilation, runtime instances, and rendering.
     *
     * Manager is the application-scoped entry point for Aether. It loads effect
     * assets, resolves their material definitions, compiles immutable system data,
     * and manages runtime system instances.
     *
     * The manager owns the Aether renderer. The renderer owns GPU allocations,
     * synchronization, simulation and draw execution.
     *
     * A frame producer creates and fills a FrameSubmission. Publishing the
     * submission transfers an immutable view of its instances to the render path.
     *
     * @note The GraphicsContext, ShaderLoader, MaterialRegistry, and MaterialSystem
     * must outlive this manager.
     *
     * @thread_safety Instance registration and frame-submission publication are
     * synchronized. Call BeginFrame() and Render() from the render-frame path.
     */
    class ELIXIR_API Manager final
    {
    public:
        /**
         * @brief Creates an Aether manager.
         *
         * @param context Provides graphics resources and frame synchronization.
         * @param shaderLoader Loads the shaders required by the particle renderer.
         * @param materialRegistry Stores default and effect-generated materials.
         * @param materialSystem Resolves material instances for rendering.
         */
        Manager(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            MaterialRegistry& materialRegistry,
            MaterialSystem& materialSystem
        );

        /**
         * @brief Destroys the manager and its owned renderer.
         *
         * Runtime instances must not be used after the manager is destroyed.
         */
        ~Manager();

        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;
        Manager(Manager&&) = delete;
        Manager& operator=(Manager&&) = delete;

        /**
         * @brief Loads an effect asset into mutable authoring data.
         *
         * This function parses the effect file. It does not resolve materials or
         * compile GPU-ready system data.
         *
         * @param filepath Path to the effect asset.
         * @return The loaded system, or null when parsing fails.
         */
        static Ref<System> LoadEffect(const std::filesystem::path& filepath);

        /**
         * @brief Resolves materials and compiles a system for runtime use.
         *
         * The method resolves effect-authored materials definitions before compiling
         * the system into an immutable SCompiledSystem.
         *
         * @param system Mutable system authoring data to compile.
         * @return The compiled system, or null when material resolution fails.
         *
         * @note This method assigns a default material when emitters have no
         * explicit material.
         */
        Ref<const SCompiledSystem> Compile(System& system) const;

        /**
         * @brief Creates and registers a runtime instance of a compiled system.
         *
         * The returned instance exposes the runtime API for transforms and
         * parameter overrides. The manager retains registration and GPU lifetime
         * ownership.
         *
         * @param system Immutable compiled system data.
         * @return The registered runtime instance.
         *
         * @pre system is not null.
         */
        Ref<SystemInstance> CreateInstance(Ref<const SCompiledSystem> system);

        /**
         * @brief Detaches a runtime instance from future frames.
         *
         * The method removes the instance from the manager and from the published
         * submission. The renderer retires its GPU allocation during a later frame
         * and releases it after the required GPU fence completes.
         *
         * @param instance Registered instance to destroy.
         * @return True when the manager owned and detached the instance.
         * @return False when instance is null or belongs to another manager.
         *
         * @warning Do not submit the instance after this method returns true.
         */
        bool DestroyInstance(const Ref<SystemInstance>& instance);

        /**
         * @brief Starts an Aether render frame.
         *
         * Updates renderer frame state and forwards pending instance retirements to
         * the renderer.
         *
         * @param timestep Elapsed time for the current frame.
         *
         * @note Call this method from the render-frame path.
         */
        void BeginFrame(const Timestep& timestep);

        /**
         * @brief Creates an empty mutable frame submission.
         *
         * A single producer fills the submission with managed system instances,
         * then publishes it for rendering.
         *
         * @return A new unsealed frame submission.
         */
        static Ref<Rendering::FrameSubmission> CreateFrameSubmission();

        /**
         * @brief Captures an instance for a frame submission.
         *
         * The method captures the instance's immutable render proxy. It does not
         * publish the submission.
         *
         * @param submission Submission to update.
         * @param instance Registered runtime instance to capture.
         * @return True when the instance was captured.
         * @return False when the instance is unmanaged, null, duplicated, or the
         * submission is sealed.
         */
        bool Submit(
            Rendering::FrameSubmission& submission,
            const Ref<SystemInstance>& instance
        ) const;

        /**
         * @brief Publishes a completed frame submission for rendering.
         *
         * The method seals the submission and removes instances that were detached
         * before publication. The renderer consumes the latest published
         * submission.
         *
         * @param submission Submission to publish.
         *
         * @pre submission is not null.
         * @warning Do not modify the submission after publishing it.
         */
        void PublishFrameSubmission(Ref<Rendering::FrameSubmission> submission);

        /**
         * @brief Simulates and renders the latest published submission.
         *
         * The method does nothing when no submission is available.
         *
         * @param camera Camera used to render particle geometry.
         *
         * @note Call this method from the render-frame path after BeginFrame().
         */
        void Render(const Camera& camera);

        /**
         * @brief Returns metrics for the most recently rendered particle frame.
         *
         * @return Read-only metrics collected during the latest Render() call.
         *
         * @note Read the result at a frame boundary after rendering completes.
         */
        const Rendering::SParticleSubmissionMetrics& GetLastSubmissionMetrics() const;

    private:
        // Returns the owned renderer and verifies that construction completed.
        Rendering::Renderer& GetRenderer() const;

        // Forwards detached instances to the renderer for fence-safe GPU retirement.
        void RetireDestroyedInstances();

        // Checks ownership while the instance registry mutex is already held.
        bool IsManagedInstance(const Ref<SystemInstance>& instance) const;

        Effect::MaterialResolver m_EffectMaterials;

        MaterialSystem& m_MaterialSystem;
        std::unordered_map<SSystemInstanceKey, Ref<SystemInstance>> m_Instances;
        mutable std::mutex m_InstancesMutex;

        Rendering::SystemInstanceRetirementQueue m_PendingRetirements;
        Rendering::FrameSubmissionPublisher m_FrameSubmissionPublisher;
        Scope<Rendering::Renderer> m_Renderer;
    };
}
