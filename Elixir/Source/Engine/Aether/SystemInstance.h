#pragma once

#include <Engine/Aether/System.h>

namespace Elixir::Aether
{
    class FrameSubmissionPublisher;
    class Manager;
    class Renderer;

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

    // Immutable runtime state captured by one rendering frame.
    class ELIXIR_API SystemInstanceSnapshot final
    {
        friend class SystemInstance;
        friend class FrameSubmission;
        friend class Manager;
        friend class Renderer;

    public:
        SystemInstanceSnapshot(
            SSystemInstanceKey key,
            uint32_t revision,
            uint32_t parameterRevision,
            Ref<const SCompiledSystem> system,
            const glm::mat4& worldTransform,
            Ref<const ParameterOverridesMap> overrides
        );

        glm::vec4 ResolveParameterValue(uint32_t parameterIndex) const;

        uint32_t GetRevision() const { return m_Revision; }
        uint32_t GetParameterRevision() const { return m_ParameterRevision; }
        const SCompiledSystem& GetCompiledSystem() const { return *m_CompiledSystem; }
        const glm::mat4& GetWorldTransform() const { return m_WorldTransform; }

    private:
        const SSystemInstanceKey& GetKey() const { return m_Key; }

        SSystemInstanceKey m_Key;
        uint32_t m_Revision = 1;
        uint32_t m_ParameterRevision = 1;
        Ref<const SCompiledSystem> m_CompiledSystem;
        glm::mat4 m_WorldTransform{ 1.0f };
        Ref<const ParameterOverridesMap> m_ParameterOverrides;
    };

    // Runtime identity and immutable compiled payload selection.
    // GPU allocations belong to Renderer::ParticleResourcePool, never here.
    class ELIXIR_API SystemInstance final
    {
        friend class FrameSubmission;
        friend class FrameSubmissionPublisher;
        friend class Manager;
        friend class Renderer;

    public:
        explicit SystemInstance(Ref<const SCompiledSystem> system);
        SystemInstance(const SystemInstance&) = delete;
        SystemInstance& operator=(const SystemInstance&) = delete;
        SystemInstance(SystemInstance&&) = delete;
        SystemInstance& operator=(SystemInstance&&) = delete;

        void SetCompiledSystem(Ref<const SCompiledSystem> system);

        void SetWorldTransform(const glm::mat4& worldTransform);

        bool SetParameterOverride(const std::string& name, const glm::vec4& value);
        bool ClearParameterOverride(const std::string& name);
        void ClearParameterOverrides();

    private:
        // Acquires one immutable view without retaining the instance lock.
        Ref<const SystemInstanceSnapshot> CaptureSnapshot() const;

        static Ref<const SystemInstanceSnapshot> CreateSnapshot(
            const SSystemInstanceKey& key,
            uint32_t revision,
            uint32_t parameterRevision,
            Ref<const SCompiledSystem> system,
            const glm::mat4& worldTransform,
            Ref<const ParameterOverridesMap> overrides
        );

        static bool IsExposedParameter(const SCompiledSystem& system, std::string_view name);

        const SSystemInstanceKey& GetKey() const { return m_Key; }

        SSystemInstanceKey m_Key;
        Ref<const SystemInstanceSnapshot> m_Snapshot;
        mutable std::mutex m_SnapshotMutex;
    };
}

GENERATE_HASH_FUNCTION(Elixir::Aether::SSystemInstanceKey)