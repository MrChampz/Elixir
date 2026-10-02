#include "epch.h"
#include "SystemInstance.h"

#include <Engine/Aether/Rendering/SystemInstanceRenderProxy.h>

namespace Elixir::Aether
{
    /* SystemInstanceSnapshot */

    SystemInstanceSnapshot::SystemInstanceSnapshot(
        SSystemInstanceKey key,
        const uint32_t revision,
        const uint32_t parameterRevision,
        Ref<const SCompiledSystem> system,
        const glm::mat4& worldTransform,
        Ref<const ResolvedParameterValues> parameters
    ) : m_Key(std::move(key)),
        m_Revision(revision),
        m_ParameterRevision(parameterRevision),
        m_CompiledSystem(std::move(system)),
        m_WorldTransform(worldTransform),
        m_Parameters(std::move(parameters)),
        m_RenderProxy(new Rendering::SystemInstanceRenderProxy(
            m_Key,
            m_Revision,
            m_ParameterRevision,
            m_CompiledSystem,
            m_WorldTransform,
            m_Parameters
        )) {}

    /* SystemInstance */

    SystemInstance::SystemInstance(Ref<System> system)
      : m_SourceSystemId(system->GetId()),
        m_SourceSystem(std::move(system))
    {
        EE_CORE_ASSERT(m_SourceSystem, "SystemInstance requires an authored system.")
    }

    bool SystemInstance::SetParameterOverride(
        const std::string& name,
        const glm::vec4& value
    )
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        const bool isExposed = m_CompiledSystem
            ? FindExposedParameter(*m_CompiledSystem, name) != nullptr
            : m_SourceSystem
                && m_SourceSystem->GetParameterDefault(name).has_value();

        if (!isExposed) return false;

        m_ParameterOverrides.insert_or_assign(name, value);
        ++m_ParameterRevision;

        if (m_CompiledSystem)
            PublishSnapshot();

        return true;
    }

    bool SystemInstance::ClearParameterOverride(const std::string& name)
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        if (m_ParameterOverrides.erase(name) == 0)
            return false;

        ++m_ParameterRevision;

        if (m_CompiledSystem)
            PublishSnapshot();

        return true;
    }

    void SystemInstance::ClearParameterOverrides()
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        if (m_ParameterOverrides.empty())
            return;

        m_ParameterOverrides.clear();
        ++m_ParameterRevision;

        if (m_CompiledSystem)
            PublishSnapshot();
    }

    std::optional<glm::vec4> SystemInstance::GetParameterValue(const std::string& name) const
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        if (!m_CompiledSystem)
        {
            if (!m_SourceSystem)
                return std::nullopt;

            const auto defaultValue = m_SourceSystem->GetParameterDefault(name);
            if (!defaultValue)
                return std::nullopt;

            const auto override = m_ParameterOverrides.find(name);
            return override != m_ParameterOverrides.end()
                ? override->second
                : *defaultValue;
        }

        const auto* exposedParameter = FindExposedParameter(*m_CompiledSystem, name);
        if (!exposedParameter) return std::nullopt;

        EE_CORE_ASSERT(
            exposedParameter->ParameterIndex < m_CompiledSystem->Parameters.size(),
            "Aether exposed parameter index is outside the compiled parameter table."
        )

        if (exposedParameter->ParameterIndex >= m_CompiledSystem->Parameters.size())
            return std::nullopt;

        return ResolveParameterValue(
            m_CompiledSystem->Parameters[exposedParameter->ParameterIndex],
            m_ParameterOverrides
        );
    }

    void SystemInstance::SetWorldTransform(const glm::mat4& worldTransform)
    {
        const std::scoped_lock lock(m_SnapshotMutex);
        m_WorldTransform = worldTransform;

        if (m_CompiledSystem)
            PublishSnapshot();
    }

    Ref<System> SystemInstance::GetSourceSystem() const
    {
        const std::scoped_lock lock(m_SnapshotMutex);
        return m_SourceSystem;
    }

    bool SystemInstance::TryBeginSubmission()
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        if (m_SubmissionStarted)
            return false;

        m_SubmissionStarted = true;
        return true;
    }

    void SystemInstance::CancelSubmission()
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        EE_CORE_ASSERT(
            !m_CompiledSystem,
            "A compiled Aether instance cannot cancel its submission."
        )

        if (!m_CompiledSystem)
            m_SubmissionStarted = false;
    }

    bool SystemInstance::Initialize(Ref<const SCompiledSystem> system)
    {
        EE_CORE_ASSERT(system, "SystemInstance requires a compiled system.")
        if (!system) return false;

        const std::scoped_lock lock(m_SnapshotMutex);

        if (!m_SubmissionStarted || m_CompiledSystem)
            return false;

        EE_CORE_ASSERT(
            system->SourceId == m_SourceSystemId,
            "Aether compilation must belong to the instance source system."
        )

        if (system->SourceId != m_SourceSystemId)
            return false;

        const auto removed = std::erase_if(
            m_ParameterOverrides,
            [&system](const auto& entry)
            {
                return FindExposedParameter(*system, entry.first) == nullptr;
            }
        );

        m_CompiledSystem = std::move(system);
        m_SourceSystem.reset();

        if (removed > 0)
            ++m_ParameterRevision;

        PublishSnapshot();
        return true;
    }

    void SystemInstance::ApplyCompilation(Ref<const SCompiledSystem> system)
    {
        EE_CORE_ASSERT(system, "SystemInstance requires a compiled system.")
        const std::scoped_lock lock(m_SnapshotMutex);

        EE_CORE_ASSERT(
            system->SourceId == m_SourceSystemId,
            "Aether compilation must belong to the instance source system."
        )

        if (system->SourceId != m_SourceSystemId)
            return;

        if (m_CompiledSystem == system)
            return;

        const auto removed = std::erase_if(m_ParameterOverrides, [&system](const auto& entry)
        {
           return FindExposedParameter(*system, entry.first) == nullptr;
        });

        m_CompiledSystem = std::move(system);
        ++m_Revision;

        if (removed > 0)
            ++m_ParameterRevision;

        PublishSnapshot();
    }

    Ref<const SystemInstanceSnapshot> SystemInstance::CaptureSnapshot() const
    {
        const std::scoped_lock lock(m_SnapshotMutex);
        return m_Snapshot;
    }

    void SystemInstance::PublishSnapshot()
    {
        EE_CORE_ASSERT(
            m_CompiledSystem,
            "SystemInstance requires compiled data before publishing a snapshot."
        )

        m_Snapshot = CreateRef<SystemInstanceSnapshot>(
            m_Key,
            m_Revision,
            m_ParameterRevision,
            m_CompiledSystem,
            m_WorldTransform,
            ResolveParameterValues(*m_CompiledSystem, m_ParameterOverrides)
        );
    }

    const SExposedParameter* SystemInstance::FindExposedParameter(
        const SCompiledSystem& system,
        std::string_view name
    )
    {
        const auto found = std::ranges::find_if(
            system.ExposedParameters,
            [&name](const SExposedParameter& parameter)
            {
                return parameter.Name == name;
            }
        );

        return found != system.ExposedParameters.end()
            ? &*found
            : nullptr;
    }

    glm::vec4 SystemInstance::ResolveParameterValue(
        const SGPUParameter& parameter,
        const ParameterOverridesMap& overrides
    )
    {
        const auto found = overrides.find(parameter.Name);
        return found != overrides.end()
            ? found->second
            : parameter.Value;
    }

    Ref<const ResolvedParameterValues> SystemInstance::ResolveParameterValues(
        const SCompiledSystem& system,
        const ParameterOverridesMap& overrides
    )
    {
        auto values = CreateRef<ResolvedParameterValues>();
        values->reserve(system.Parameters.size());

        for (const auto& parameter : system.Parameters)
            values->push_back(ResolveParameterValue(parameter, overrides));

        return values;
    }
}
