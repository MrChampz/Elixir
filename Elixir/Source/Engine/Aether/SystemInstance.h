#pragma once

#include <Engine/Aether/System.h>

namespace Elixir::Aether
{
    using ParameterOverridesMap = std::unordered_map<std::string, glm::vec4>;

    // Immutable runtime state captured by one rendering frame.
    class ELIXIR_API SystemInstanceSnapshot final
    {
        friend class SystemInstance;

    public:
        SystemInstanceSnapshot(
            UUID id,
            uint32_t revision,
            uint32_t parameterRevision,
            Ref<const SCompiledSystem> system,
            const glm::mat4& worldTransform,
            Ref<const ParameterOverridesMap> overrides
        );

        glm::vec4 ResolveParameterValue(uint32_t parameterIndex) const;

        const UUID& GetId() const { return m_Id; }
        uint32_t GetRevision() const { return m_Revision; }
        uint32_t GetParameterRevision() const { return m_ParameterRevision; }
        const SCompiledSystem& GetCompiledSystem() const { return *m_CompiledSystem; }
        const glm::mat4& GetWorldTransform() const { return m_WorldTransform; }

    private:
        UUID m_Id;
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
    public:
        explicit SystemInstance(Ref<const SCompiledSystem> compiledSystem);
        SystemInstance(const SystemInstance&) = delete;
        SystemInstance& operator=(const SystemInstance&) = delete;
        SystemInstance(SystemInstance&&) = delete;
        SystemInstance& operator=(SystemInstance&&) = delete;

        const UUID& GetId() const { return m_Id; }

        void SetCompiledSystem(Ref<const SCompiledSystem> compiledSystem);

        void SetWorldTransform(const glm::mat4& worldTransform);

        bool SetParameterOverride(const std::string& name, const glm::vec4& value);
        bool ClearParameterOverride(const std::string& name);
        void ClearParameterOverrides();

        // Acquire one immutable view without retaining the instance lock.
        Ref<const SystemInstanceSnapshot> CaptureSnapshot() const;

    private:
        static Ref<const SystemInstanceSnapshot> CreateSnapshot(
            const UUID& id,
            uint32_t revision,
            uint32_t parameterRevision,
            Ref<const SCompiledSystem> system,
            const glm::mat4& worldTransform,
            Ref<const ParameterOverridesMap> overrides
        );

        static bool IsExposedParameter(const SCompiledSystem& system, std::string_view name);

        UUID m_Id;

        mutable std::mutex m_SnapshotMutex;
        Ref<const SystemInstanceSnapshot> m_Snapshot;
    };
}
