#pragma once

#include <Engine/Aether/SystemInstance.h>
#include <Engine/Aether/Effect/MaterialResolver.h>
#include <Engine/Aether/Runtime/InstanceRegistry.h>
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

    namespace Aether
    {
        namespace Simulation
        {
            class Simulator;
            struct SSimulationMetrics;
        }

        namespace Rendering
        {
            class Renderer;
            struct SRenderingMetrics;
        }
    }
}

namespace Elixir::Aether
{
    using namespace Runtime;
    using namespace Simulation;
    using namespace Rendering;

    /**
     * @brief Coordinates Aether effects, runtime instances, simulation, and rendering.
     *
     * Manager is the application-scoped entry point for Aether. It loads effect
     * assets, resolves their material definitions, compiles immutable system data,
     * and manages runtime system instances.
     *
     * Manager owns Simulator and Renderer. Simulator owns particle allocations,
     * simulation resources, compute pipelines, and deferred resource retirement.
     * Renderer consumes immutable render frames and records particle draw commands.
     *
     * A frame producer creates and fills a FrameSubmission. Publishing the
     * submission makes its immutable instance data available to Simulator.
     * Simulator produces a RenderFrame for Renderer.
     *
     * @note GraphicsContext, MaterialRegistry, and MaterialSystem must outlive
     * this manager.
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
         * @param context Graphics context used for simulation and rendering.
         * @param shaderLoader Loader used to create the simulation shaders.
         * @param materialRegistry Stores default and effect-generated materials.
         * @param materialSystem Compiles and renders particle materials.
         *
         * @pre context is not null and outlives the manager.
         * @pre shaderLoader is not null.
         * @pre materialRegistry outlives the manager.
         * @pre materialSystem outlives the manager.
         */
        Manager(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            MaterialRegistry& materialRegistry,
            MaterialSystem& materialSystem
        );

        /**
         * @brief Destroys the manager and its simulation and rendering services.
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
         * @brief Creates and registers an instance of a System asset.
         *
         * Runtime compiles and caches the asset without retaining the authored
         * System.
         *
         * @param system System asset to instantiate.
         * @return Registered instance, or null when compilation fails.
         */
        Ref<SystemInstance> CreateInstance(const Ref<System>& system);

        /**
         * @brief Recompiles a System and updates its registered instances.
         *
         * @param system System asset to recompile.
         * @return True when compilation succeeds.
         */
        bool Recompile(const Ref<System>& system);

        /**
         * @brief Detaches a runtime instance from future frames.
         *
         * The method removes the instance from Manager and from the published
         * submission. Simulator retires the GPU allocation during a later frame
         * and releases it after the GPU finishes using it.
         *
         * @param instance Registered instance to destroy.
         * @return True when the manager owned and detached the instance.
         * @return False when instance is null or belongs to another manager.
         *
         * @note Submit() rejects the instance after this method returns true.
         */
        bool DestroyInstance(const Ref<SystemInstance>& instance);

        /**
         * @brief Prepares Aether for a new frame.
         *
         * The method updates simulation time, releases allocations whose GPU work
         * has completed, and forwards destroyed instances to Simulator.
         *
         * @param timestep Elapsed time for the current frame.
         *
         * @note Call this method once per frame, after the graphics context prepares
         * the current frame slot and before Render().
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
        static Ref<FrameSubmission> CreateFrameSubmission();

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
            FrameSubmission& submission,
            const Ref<SystemInstance>& instance
        ) const;

        /**
         * @brief Publishes a completed frame submission for rendering.
         *
         * The method seals the submission and removes instances that Manager no
         * longer owns. Simulator consumes the latest published submission when
         * Render() runs.
         *
         * @param submission Submission to publish.
         *
         * @pre submission is not null.
         * @warning Do not modify the submission after publishing it.
         */
        void PublishFrameSubmission(Ref<FrameSubmission> submission);

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
         * @brief Returns statistics from the most recent particle simulation.
         * @return Statistics for the latest submission processed by Simulator.
         * @note Processing another submission replaces these values.
         */
        const SSimulationMetrics& GetLastSimulationMetrics() const;

        /**
         * @brief Returns statistics from the most recent particle rendering operation.
         * @return Statistics for the latest RenderFrame processed by Renderer.
         * @note Rendering another frame replaces these values.
         */
        const SRenderingMetrics& GetLastRenderingMetrics() const;

    private:
        InstanceRegistry& GetRuntime() const;
        Simulator& GetSimulator() const;
        Renderer& GetRenderer() const;

        // Forwards detached instances to Simulator for fence-safe GPU retirement.
        void RetireDestroyedInstances();

        Scope<InstanceRegistry> m_Runtime;
        Scope<Simulator> m_Simulator;

        SystemInstanceRetirementQueue m_PendingRetirements;
        Scope<Renderer> m_Renderer;

        const GraphicsContext* m_GraphicsContext = nullptr;
    };
}
