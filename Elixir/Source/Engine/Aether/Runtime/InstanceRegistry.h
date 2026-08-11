#pragma once

#include <Engine/Aether/SystemInstance.h>
#include <Engine/Aether/Effect/MaterialResolver.h>
#include <Engine/Aether/Rendering/FrameSubmission.h>

namespace Elixir
{
    class MaterialRegistry;
    class MaterialResolver;
}

namespace Elixir::Aether::Runtime
{
    using Rendering::FrameSubmission;
    using Rendering::FrameSubmissionPublisher;

    /**
     * @brief Manages compiled Aether systems and their runtime instances.
     *
     * InstanceRegistry resolves effect materials, compiles System assets, caches
     * immutable runtime data, and owns registered SystemInstance objects. It also
     * publishes immutable frame submissions without depending on GPU services.
     *
     * The registry does not retain authored System assets. A caller may release a
     * System after creating its instances. The compiled representation remains
     * available until this registry is destroyed.
     *
     * @note MaterialRegistry and MaterialResolver must outlive this registry.
     *
     * @thread_safety Public methods synchronize compilation, instance ownership,
     * and frame publication.
     */
    class ELIXIR_API InstanceRegistry final
    {
    public:
        /**
         * @brief Creates an instance registry.
         *
         * @param materialRegistry Stores effect-authored and default materials.
         * @param materialResolver Compiles material instances into render proxies.
         */
        InstanceRegistry(
            MaterialRegistry& materialRegistry,
            MaterialResolver& materialResolver
        );

        /**
         * @brief Creates and registers an instance of a System asset.
         *
         * The method reuses an existing compiled representation when available.
         * It does not retain the authored System.
         *
         * @param system System asset to instantiate.
         * @return Registered instance, or null when compilation fails.
         */
        Ref<SystemInstance> CreateInstance(const Ref<System>& system);

        /**
         * @brief Recompiles a System and updates all of its registered instances.
         *
         * Compatible parameter overrides remain active. The method replaces the
         * cached compilation and publishes a new snapshot from each affected
         * instance.
         *
         * @param system System asset to recompile.
         * @return True when the compilation succeeds.
         */
        bool Recompile(const Ref<System>& system);

        /**
         * @brief Detaches a registered instance from the runtime.
         *
         * The method removes the instance from future frame submissions. It does
         * not release GPU resources.
         *
         * @param instance Instance to detach.
         * @return Detached instance, or null when it is not registered.
         */
        Ref<SystemInstance> DetachInstance(const Ref<SystemInstance>& instance);

        /**
         * @brief Adds a registered instance to a frame submission.
         *
         * @param submission Submission to update.
         * @param instance Instance to capture.
         * @return True when the instance was captured.
         */
        bool Submit(FrameSubmission& submission, const Ref<SystemInstance>& instance);

        /**
         * @brief Publishes a frame submission containing managed instances.
         *
         * Instances detached before publication are removed from the published
         * submission.
         *
         * @param submission Submission to publish.
         */
        void Publish(Ref<FrameSubmission> submission);

        /**
         * @brief Returns the latest immutable frame submission.
         * @return Published submission, or null when none is available.
         */
        Ref<const FrameSubmission> AcquireSubmission();

    private:
        // Compiles one authored System into immutable runtime data.
        Ref<const SCompiledSystem> CompileSystem(const System& system) const;

        // Checks ownership while m_Mutex is already held.
        bool IsManagedInstance(const Ref<SystemInstance>& instance) const;

        Effect::MaterialResolver m_EffectMaterials;
        MaterialResolver& m_MaterialResolver;

        std::unordered_map<UUID, Ref<const SCompiledSystem>> m_CompiledSystems;
        std::unordered_map<SSystemInstanceKey, Ref<SystemInstance>> m_Instances;

        mutable std::mutex m_Mutex;
        FrameSubmissionPublisher m_Publisher;
    };
}
