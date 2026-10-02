#pragma once

#include <Engine/Aether/System.h>

namespace Elixir::Aether
{
    namespace Runtime { class InstanceRegistry; }
    namespace Rendering
    {
        class FrameSubmission;
        class FrameSubmissionPublisher;
        class Renderer;
        class SystemInstanceRenderProxy;
    }
}

namespace Elixir::Aether
{
    class Manager;

    /**
     * @brief Identifies one runtime SystemInstance inside Aether.
     *
     * The key is created and used only by Aether internals. It lets Manager,
     * FrameSubmission, and Renderer refer to the same instance without exposing a
     * public handle API.
     *
     * @note Equality and hashing use the instance UUID.
     */
    struct SSystemInstanceKey
    {
        friend class SystemInstance;
        friend class Manager;

        std::size_t GetHashParams() const
        {
            return m_Id.GetHashParams();
        }

        bool operator==(const SSystemInstanceKey&) const = default;

    private:
        SSystemInstanceKey() = default;

        UUID m_Id;
    };

    using ParameterOverridesMap = std::unordered_map<std::string, glm::vec4>;
    using ResolvedParameterValues = std::vector<glm::vec4>;

    /**
     * @brief Stores one immutable view of a SystemInstance.
     *
     * A snapshot contains the compiled system, world transform, and resolved
     * parameter values selected at one point in time. It also creates the
     * corresponding SystemInstanceRenderProxy for the renderer.
     *
     * SystemInstance publishes a replacement snapshot after each accepted change.
     * Existing snapshots remain valid for frame submissions that already captured
     * them.
     *
     * @thread_safety Immutable after construction.
     */
    class ELIXIR_API SystemInstanceSnapshot final
    {
        friend class Manager;
        friend class SystemInstance;
        friend class Rendering::Renderer;
        friend class Rendering::FrameSubmission;

    public:
        /**
         * @brief Creates an immutable runtime-state snapshot.
         *
         * @param key Internal identity of the source instance.
         * @param revision Revision of the compiled-system selection.
         * @param parameterRevision Revision of the resolved parameter values.
         * @param system Compiled system selected by the instance.
         * @param worldTransform World transform selected by the instance.
         * @param parameters Resolved values in compiled parameter order.
         *
         * @pre system is not null.
         * @pre parameters is not null.
         */
        SystemInstanceSnapshot(
            SSystemInstanceKey key,
            uint32_t revision,
            uint32_t parameterRevision,
            Ref<const SCompiledSystem> system,
            const glm::mat4& worldTransform,
            Ref<const ResolvedParameterValues> parameters
        );

        /**
         * @brief Returns the compiled-system selection revision.
         * @return Revision incremented when the compiled system changes.
         */
        uint32_t GetRevision() const { return m_Revision; }

        /**
         * @brief Returns the resolved parameter-value revision.
         * @return Revision incremented when effective parameter values change.
         */
        uint32_t GetParameterRevision() const { return m_ParameterRevision; }

        /**
         * @brief Returns the selected immutable compiled system.
         * @return Compiled system used by this snapshot.
         */
        const SCompiledSystem& GetCompiledSystem() const { return *m_CompiledSystem; }

        /**
         * @brief Returns the selected world transform.
         * @return World transform stored in this snapshot.
         */
        const glm::mat4& GetWorldTransform() const { return m_WorldTransform; }

    private:
        // Returns the internal identity used by frame and renderer bookkeeping.
        const SSystemInstanceKey& GetKey() const { return m_Key; }

        // Returns the immutable renderer-facing state derived from this snapshot.
        const Ref<const Rendering::SystemInstanceRenderProxy>& GetRenderProxy() const
        {
            return m_RenderProxy;
        }

        SSystemInstanceKey m_Key;
        uint32_t m_Revision = 1;
        uint32_t m_ParameterRevision = 1;
        Ref<const SCompiledSystem> m_CompiledSystem;
        glm::mat4 m_WorldTransform{ 1.0f };
        Ref<const ResolvedParameterValues> m_Parameters;
        Ref<const Rendering::SystemInstanceRenderProxy> m_RenderProxy;
    };

    /**
     * @brief Represents one runtime use of an Aether system.
     *
     * System::CreateInstance() creates an unregistered instance from authored
     * system data. Manager::Submit() compiles and registers it on first use.
     *
     * The instance owns mutable runtime choices such as its world transform and
     * exposed parameter overrides. After registration, it publishes each change
     * as an immutable SystemInstanceSnapshot.
     *
     * The instance does not own particle buffers or other GPU resources. Manager
     * registers the instance, and Renderer owns its GPU allocation and retirement.
     *
     * @thread_safety Public mutation methods synchronize snapshot publication.
     * Frame submission captures an immutable snapshot and never reads mutable
     * instance state from the render thread.
     */
    class ELIXIR_API SystemInstance final
    {
        friend class Manager;
        friend class System;
        friend class Runtime::InstanceRegistry;
        friend class Rendering::Renderer;
        friend class Rendering::FrameSubmission;
        friend class Rendering::FrameSubmissionPublisher;

    public:
        /**
         * @brief Creates an unregistered from an authored System.
         * @param system Authored system to instantiate.
         */
        explicit SystemInstance(Ref<System> system);

        SystemInstance(const SystemInstance&) = delete;
        SystemInstance& operator=(const SystemInstance&) = delete;
        SystemInstance(SystemInstance&&) = delete;
        SystemInstance& operator=(SystemInstance&&) = delete;

        /**
         * @brief Sets an override for an exposed compiled-system parameter.
         *
         * @param name Name of the exposed parameter.
         * @param value Replacement float4 value.
         * @return True when the parameter is exposed and the override was stored.
         * @return False when the parameter is not exposed.
         */
        bool SetParameterOverride(const std::string& name, const glm::vec4& value);

        /**
         * @brief Removes one parameter override.
         * @param name Name of the overridden parameter.
         * @return True when an override was removed.
         * @return False when no override exists for this name.
         */
        bool ClearParameterOverride(const std::string& name);

        /**
         * @brief Removes all parameter overrides.
         *
         * The method does nothing when no overrides are set.
         */
        void ClearParameterOverrides();

        /**
         * @brief Returns the effective value of an exposed parameter.
         *
         * The method returns the instance override when one exists. Otherwise, it
         * returns the default value from the selected compiled system.
         *
         * @param name Name of the exposed parameter.
         * @return Effective value, or no value when the parameter is not exposed.
         */
        std::optional<glm::vec4> GetParameterValue(const std::string& name) const;

        /**
         * @brief Returns the identity of the source System.
         * @return UUID of the System used to create this instance.
         */
        const UUID& GetSourceSystemId() const { return m_SourceSystemId; }

        /**
         * @brief Replaces the world transform for future frame submissions.
         * @param worldTransform Transform applied to this system instance.
         */
        void SetWorldTransform(const glm::mat4& worldTransform);

    private:
        // Returns the authored System retained before the first submission.
        Ref<System> GetSourceSystem() const;

        // Reserves the instance for its first runtime submission.
        bool TryBeginSubmission();

        // Cancels a failed first submission so that it can be retried.
        void CancelSubmission();

        // Applies the first compilation and releases the authored System.
        bool Initialize(Ref<const SCompiledSystem> system);

        // Replaces the internal compilation after rebuilding the same source asset.
        void ApplyCompilation(Ref<const SCompiledSystem> system);

        // Captures the current immutable state without retaining the mutex.
        Ref<const SystemInstanceSnapshot> CaptureSnapshot() const;

        // Publishes the current mutable state as an immutable snapshot.
        void PublishSnapshot();

        // Finds the compiled mapping for an exposed runtime parameter.
        static const SExposedParameter* FindExposedParameter(
            const SCompiledSystem& system,
            std::string_view name
        );

        // Resolves one compiled parameter against the instance overrides.
        static glm::vec4 ResolveParameterValue(
            const SGPUParameter& parameter,
            const ParameterOverridesMap& overrides
        );

        // Resolves all compiled parameters into immutable renderer table order.
        static Ref<const ResolvedParameterValues> ResolveParameterValues(
            const SCompiledSystem& system,
            const ParameterOverridesMap& overrides
        );

        // Returns the internal identity used by Manager and Renderer.
        const SSystemInstanceKey& GetKey() const { return m_Key; }

        SSystemInstanceKey m_Key;
        UUID m_SourceSystemId;
        uint32_t m_Revision = 1;
        uint32_t m_ParameterRevision = 1;
        Ref<System> m_SourceSystem;
        Ref<const SCompiledSystem> m_CompiledSystem;
        glm::mat4 m_WorldTransform{ 1.0f };
        ParameterOverridesMap m_ParameterOverrides;

        bool m_SubmissionStarted = false;
        Ref<const SystemInstanceSnapshot> m_Snapshot;
        mutable std::mutex m_SnapshotMutex;
    };
}

GENERATE_HASH_FUNCTION(Elixir::Aether::SSystemInstanceKey)
