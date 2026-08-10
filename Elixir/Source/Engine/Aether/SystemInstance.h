#pragma once

#include <Engine/Aether/System.h>

namespace Elixir::Aether::Rendering
{
    class FrameSubmission;
    class FrameSubmissionPublisher;
    class Renderer;
    class SystemInstanceRenderProxy;
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

    /**
     * @brief Stores one immutable view of a SystemInstance.
     *
     * A snapshot contains the compiled system, world transform, and parameter
     * overrides selected at one point in time. It also creates the corresponding
     * SystemInstanceRenderProxy for the renderer.
     *
     * SystemInstance publishes a replacement snapshot after each accepted change.
     * Existing snapshots remain valid for frame submissions that already captured
     * them.
     *
     * @thread_safety Immutable after construction.
     */
    class ELIXIR_API SystemInstanceSnapshot final
    {
        friend class SystemInstance;
        friend class Rendering::FrameSubmission;
        friend class Manager;
        friend class Rendering::Renderer;

    public:
        /**
         * @brief Creates an immutable runtime-state snapshot.
         *
         * @param key Internal identity of the source instance.
         * @param revision Revision of the compiled-system selection.
         * @param parameterRevision Revision of the resolved parameter values.
         * @param system Compiled system selected by the instance.
         * @param worldTransform World transform selected by the instance.
         * @param overrides Named parameter overrides selected by the instance.
         *
         * @pre system is not null.
         * @pre overrides is not null.
         */
        SystemInstanceSnapshot(
            SSystemInstanceKey key,
            uint32_t revision,
            uint32_t parameterRevision,
            Ref<const SCompiledSystem> system,
            const glm::mat4& worldTransform,
            Ref<const ParameterOverridesMap> overrides
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
        Ref<const ParameterOverridesMap> m_ParameterOverrides;
        Ref<const Rendering::SystemInstanceRenderProxy> m_RenderProxy;
    };

    /**
     * @brief Represents one runtime use of a compiled Aether system.
     *
     * SystemInstance owns mutable runtime choices: the selected compiled system,
     * world transform, and exposed parameter overrides. It publishes each change
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
        friend class Rendering::FrameSubmission;
        friend class Rendering::FrameSubmissionPublisher;
        friend class Manager;
        friend class Rendering::Renderer;

    public:
        /**
         * @brief Creates a runtime instance for a compiled system.
         *
         * @param system Immutable compiled system to select initially.
         *
         * @pre system is not null.
         */
        explicit SystemInstance(Ref<const SCompiledSystem> system);

        SystemInstance(const SystemInstance&) = delete;
        SystemInstance& operator=(const SystemInstance&) = delete;
        SystemInstance(SystemInstance&&) = delete;
        SystemInstance& operator=(SystemInstance&&) = delete;

        /**
         * @brief Replaces the selected compiled system.
         *
         * Compatible parameter overrides are preserved. Overrides that do not
         * exist in the replacement system are removed.
         *
         * @param system Replacement compiled system.
         *
         * @pre system is not null.
         */
        void SetCompiledSystem(Ref<const SCompiledSystem> system);

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
         * @brief Replaces the world transform for future frame submissions.
         * @param worldTransform Transform applied to this system instance.
         */
        void SetWorldTransform(const glm::mat4& worldTransform);

    private:
        // Captures the current immutable state without retaining the mutex.
        Ref<const SystemInstanceSnapshot> CaptureSnapshot() const;

        // Builds a snapshot and its renderer-facing proxy from resolved state.
        static Ref<const SystemInstanceSnapshot> CreateSnapshot(
            const SSystemInstanceKey& key,
            uint32_t revision,
            uint32_t parameterRevision,
            Ref<const SCompiledSystem> system,
            const glm::mat4& worldTransform,
            Ref<const ParameterOverridesMap> overrides
        );

        // Checks whether a parameter can be changed through the runtime API.
        static bool IsExposedParameter(const SCompiledSystem& system, std::string_view name);

        // Returns the internal identity used by Manager and Renderer.
        const SSystemInstanceKey& GetKey() const { return m_Key; }

        SSystemInstanceKey m_Key;
        Ref<const SystemInstanceSnapshot> m_Snapshot;
        mutable std::mutex m_SnapshotMutex;
    };
}

GENERATE_HASH_FUNCTION(Elixir::Aether::SSystemInstanceKey)
