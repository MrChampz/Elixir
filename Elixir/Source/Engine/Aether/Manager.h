#pragma once

#include <Engine/Aether/SystemInstance.h>
#include <Engine/Aether/Rendering/SystemInstanceRetirementQueue.h>

namespace Elixir
{
    class Camera;
    class GraphicsContext;
    class ShaderLoader;
    class Timestep;

    namespace Materials
    {
        class MaterialSystem;
        class MaterialRegistry;

        namespace Rendering
        {
            class Resolver;
            class RenderContext;
        }
    }

    namespace Aether
    {
        namespace Runtime { class InstanceRegistry; }

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
    using namespace Materials;

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
     * Systems create unregistered SystemInstance objects. Submit() registers each
     * instance once and keeps it active until DestroyInstance() removes it.
     * BeginFrame() publishes immutable state for every active instance.
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
         * @brief Recompiles a System and updates its registered instances.
         *
         * @param system System asset to recompile.
         * @return True when compilation succeeds.
         */
        bool Recompile(const Ref<System>& system);

        /**
         * @brief Registers an instance for persistent simulation and rendering.
         *
         * The method compiles and activates an instance created by
         * System::CreateInstance(). A successful submission remains active in
         * every frame until DestroyInstance() removes it.
         *
         * @param instance Unregistered runtime instance to activate.
         * @return True when the instance was registered.
         * @return False when the instance is null or was previously submitted.
         *
         * @thread_safety May be called from any thread. Calls are serialized with
         * active-instance publication.
         */
        bool Add(const Ref<SystemInstance>& instance);

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
         * @note RegisterInstance() rejects this instance after the method returns true.
         */
        bool Remove(const Ref<SystemInstance>& instance);

        /**
         * @brief Prepares Aether for a new frame.
         *
         * The method captures all active instances, updates simulation time,
         * releases completed allocations, and forwards destroyed instances to
         * Simulator.
         *
         * @param timestep Elapsed time for the current frame.
         *
         * @note Call this method once per frame, after the graphics context prepares
         * the current frame slot.
         */
        void BeginFrame(const Timestep& timestep);

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
        Scope<Renderer> m_Renderer;

        SystemInstanceRetirementQueue m_PendingRetirements;

        const GraphicsContext* m_GraphicsContext = nullptr;
    };
}
