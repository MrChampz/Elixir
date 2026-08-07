#include "epch.h"
#include "SystemInstance.h"

namespace Elixir::Aether
{
    /* SystemInstanceSnapshot */

    SystemInstanceSnapshot::SystemInstanceSnapshot(
        SSystemInstanceKey key,
        const uint32_t revision,
        const uint32_t parameterRevision,
        Ref<const SCompiledSystem> system,
        const glm::mat4& worldTransform,
        Ref<const ParameterOverridesMap> overrides
    ) : m_Key(std::move(key)),
        m_Revision(revision),
        m_ParameterRevision(parameterRevision),
        m_CompiledSystem(std::move(system)),
        m_WorldTransform(worldTransform),
        m_ParameterOverrides(std::move(overrides)) {}

    glm::vec4 SystemInstanceSnapshot::ResolveParameterValue(uint32_t parameterIndex) const
    {
        EE_CORE_ASSERT(
            parameterIndex < m_CompiledSystem->Parameters.size(),
            "Aether parameter index is outside the compiled system parameter table."
        )

        if (parameterIndex >= m_CompiledSystem->Parameters.size()) return {};

        const auto& parameter = m_CompiledSystem->Parameters[parameterIndex];
        const auto found = m_ParameterOverrides->find(parameter.Name);

        return found != m_ParameterOverrides->end()
            ? found->second
            : parameter.Value;
    }

    /* SystemInstance */

    SystemInstance::SystemInstance(Ref<const SCompiledSystem> system)
    {
        EE_CORE_ASSERT(system, "SystemInstance requires a compiled system.")
        m_Snapshot = CreateSnapshot(
            m_Key,
            1,
            1,
            std::move(system),
            glm::mat4{ 1.0f },
            CreateRef<ParameterOverridesMap>()
        );
    }

    void SystemInstance::SetCompiledSystem(Ref<const SCompiledSystem> system)
    {
        EE_CORE_ASSERT(system, "SystemInstance requires a compiled system.")
        const std::scoped_lock lock(m_SnapshotMutex);

        if (m_Snapshot->m_CompiledSystem == system)
            return;

        auto overrides = CreateRef<ParameterOverridesMap>(
            *m_Snapshot->m_ParameterOverrides
        );

        const auto removed = std::erase_if(*overrides, [&system](const auto& entry)
        {
           return !IsExposedParameter(*system, entry.first);
        });

        m_Snapshot = CreateSnapshot(
            m_Key,
            m_Snapshot->m_Revision + 1,
            m_Snapshot->m_ParameterRevision + (removed > 0 ? 1 : 0),
            std::move(system),
            m_Snapshot->m_WorldTransform,
            std::move(overrides)
        );
    }

    void SystemInstance::SetWorldTransform(const glm::mat4& worldTransform)
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        m_Snapshot = CreateSnapshot(
            m_Key,
            m_Snapshot->m_Revision,
            m_Snapshot->m_ParameterRevision,
            m_Snapshot->m_CompiledSystem,
            worldTransform,
            m_Snapshot->m_ParameterOverrides
        );
    }

    bool SystemInstance::SetParameterOverride(
        const std::string& name,
        const glm::vec4& value
    )
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        if (!IsExposedParameter(*m_Snapshot->m_CompiledSystem, name))
            return false;

        auto overrides = CreateRef<ParameterOverridesMap>(
            *m_Snapshot->m_ParameterOverrides
        );

        overrides->insert_or_assign(std::string(name), value);
        m_Snapshot = CreateSnapshot(
            m_Key,
            m_Snapshot->m_Revision,
            m_Snapshot->m_ParameterRevision + 1,
            m_Snapshot->m_CompiledSystem,
            m_Snapshot->m_WorldTransform,
            std::move(overrides)
        );

        return true;
    }

    bool SystemInstance::ClearParameterOverride(const std::string& name)
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        const auto found = m_Snapshot->m_ParameterOverrides->find(name);
        if (found == m_Snapshot->m_ParameterOverrides->end())
            return false;

        auto overrides = CreateRef<ParameterOverridesMap>(
            *m_Snapshot->m_ParameterOverrides
        );

        overrides->erase(found->first);

        m_Snapshot = CreateSnapshot(
            m_Key,
            m_Snapshot->m_Revision,
            m_Snapshot->m_ParameterRevision + 1,
            m_Snapshot->m_CompiledSystem,
            m_Snapshot->m_WorldTransform,
            std::move(overrides)
        );

        return true;
    }

    void SystemInstance::ClearParameterOverrides()
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        if (m_Snapshot->m_ParameterOverrides->empty())
            return;

        m_Snapshot = CreateSnapshot(
            m_Key,
            m_Snapshot->m_Revision,
            m_Snapshot->m_ParameterRevision + 1,
            m_Snapshot->m_CompiledSystem,
            m_Snapshot->m_WorldTransform,
            CreateRef<ParameterOverridesMap>()
        );
    }

    Ref<const SystemInstanceSnapshot> SystemInstance::CaptureSnapshot() const
    {
        const std::scoped_lock lock(m_SnapshotMutex);
        return m_Snapshot;
    }

    Ref<const SystemInstanceSnapshot> SystemInstance::CreateSnapshot(
        const SSystemInstanceKey& key,
        uint32_t revision,
        uint32_t parameterRevision,
        Ref<const SCompiledSystem> system,
        const glm::mat4& worldTransform,
        Ref<const ParameterOverridesMap> overrides
    )
    {
        return CreateRef<SystemInstanceSnapshot>(
            key,
            revision,
            parameterRevision,
            std::move(system),
            worldTransform,
            std::move(overrides)
        );
    }

    bool SystemInstance::IsExposedParameter(const SCompiledSystem& system, std::string_view name)
    {
        return std::ranges::any_of(
            system.ExposedParameters,
            [&name](const SExposedParameter& parameter)
            {
                return parameter.Name == name;
            }
        );
    }
}
