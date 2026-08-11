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

    SystemInstance::SystemInstance(Ref<const SCompiledSystem> system)
      : m_SourceSystemId(system->SourceId),
        m_CompiledSystem(std::move(system))
    {
        EE_CORE_ASSERT(m_CompiledSystem, "SystemInstance requires a compiled system.")
        PublishSnapshot();
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

    bool SystemInstance::SetParameterOverride(
        const std::string& name,
        const glm::vec4& value
    )
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        if (!FindExposedParameter(*m_CompiledSystem, name))
            return false;

        m_ParameterOverrides.insert_or_assign(name, value);
        ++m_ParameterRevision;
        PublishSnapshot();

        return true;
    }

    bool SystemInstance::ClearParameterOverride(const std::string& name)
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        if (m_ParameterOverrides.erase(name) == 0)
            return false;

        ++m_ParameterRevision;
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
        PublishSnapshot();
    }

    std::optional<glm::vec4> SystemInstance::GetParameterValue(const std::string& name) const
    {
        const std::scoped_lock lock(m_SnapshotMutex);

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
        PublishSnapshot();
    }

    Ref<const SystemInstanceSnapshot> SystemInstance::CaptureSnapshot() const
    {
        const std::scoped_lock lock(m_SnapshotMutex);
        return m_Snapshot;
    }

    void SystemInstance::PublishSnapshot()
    {
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
