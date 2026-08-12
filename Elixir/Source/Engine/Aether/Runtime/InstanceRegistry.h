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
     * InstanceRegistry accepts unregistered SystemInstance objects, resolves their
     * effect materials, caches compiled systems, and maintains the persistent set
     * of active instances.
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
         * @brief Compiles, registers, and activates an instance.
         *
         * The instance remains active in every subsequent frame until it is
         * detached. The first successful submission releases its authored System.
         *
         * @param instance Instance to capture.
         * @return True when the instance was registered.
         * @return False when the instance is null or was previously submitted.
         *
         * @thread_safety Concurrent calls are serialized. Exactly one concurrent
         * submission of the same instance can succeed.
         */
        bool Register(const Ref<SystemInstance>& instance);

        /**
         * @brief Detaches a registered instance from the runtime.
         *
         * The method removes the instance from future frame submissions. It does
         * not release GPU resources.
         *
         * @param instance Instance to detach.
         * @return Detached instance, or null when it is not registered.
         */
        Ref<SystemInstance> Unregister(const Ref<SystemInstance>& instance);

        /**
         * @brief Publishes the current state of every active instance.
         *
         * A  submission racing this method is included in either this frame or the
         * next frame, according to the registry lock acquisition order.
         */
        void PublishActiveInstances();

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
        std::vector<SSystemInstanceKey> m_InstanceOrder;

        mutable std::mutex m_Mutex;
        FrameSubmissionPublisher m_Publisher;
    };
}
