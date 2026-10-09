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
        Ref<const ResolvedParameterWords> parameterWords
    ) : m_Key(std::move(key)),
        m_Revision(revision),
        m_ParameterRevision(parameterRevision),
        m_CompiledSystem(std::move(system)),
        m_WorldTransform(worldTransform),
        m_ParameterWords(std::move(parameterWords)),
        m_RenderProxy(new Rendering::SystemInstanceRenderProxy(
            m_Key,
            m_Revision,
            m_ParameterRevision,
            m_CompiledSystem,
            m_WorldTransform,
            m_ParameterWords
        )) {}

    /* SystemInstance */

    SystemInstance::SystemInstance(Ref<System> system)
      : m_SourceSystemId(system->GetId()),
        m_SourceSystem(std::move(system))
    {
        EE_CORE_ASSERT(m_SourceSystem, "SystemInstance requires an authored system.")
    }

    bool SystemInstance::SetParameterOverride(const std::string& name, const float value)
    {
        return StoreParameterOverride(name, glm::vec4(value, 0.0f, 0.0f, 0.0f), EParameterType::Scalar);
    }

    bool SystemInstance::SetParameterOverride(const std::string& name, const glm::vec2& value)
    {
        return StoreParameterOverride(
            name,
            glm::vec4(value, 0.0f, 0.0f),
            EParameterType::Vector2
        );
    }

    bool SystemInstance::SetParameterOverride(const std::string& name, const glm::vec3& value)
    {
        return StoreParameterOverride(
            name,
            glm::vec4(value, 0.0f),
            EParameterType::Vector3
        );
    }

    bool SystemInstance::SetParameterOverride(const std::string& name, const glm::vec4& value)
    {
        return StoreParameterOverride(name, value, EParameterType::Vector4);
    }

    bool SystemInstance::SetParameterOverride(const std::string& name, const uint32_t value)
    {
        return StoreParameterOverride(
            name,
            EncodeUIntParameter(value),
            EParameterType::UInt
        );
    }

    const SParameter* SystemInstance::FindParameterDefinition(
        const std::string& name,
        std::optional<SParameter>& authored
    ) const
    {
        if (!m_CompiledSystem)
        {
            if (!m_SourceSystem) return nullptr;
            authored = m_SourceSystem->GetParameterDefault(name);
            return authored ? &*authored : nullptr;
        }

        const auto* exposed = FindExposedParameter(*m_CompiledSystem, name);
        if (!exposed) return nullptr;

        EE_CORE_ASSERT(
            exposed->ParameterIndex < m_CompiledSystem->Parameters.size(),
            "Aether exposed parameter index is outside the compiled parameter table."
        )

        return exposed->ParameterIndex < m_CompiledSystem->Parameters.size()
            ? &m_CompiledSystem->Parameters[exposed->ParameterIndex]
            : nullptr;
    }

    bool SystemInstance::StoreParameterOverride(
        const std::string& name,
        const glm::vec4& value,
        const std::optional<EParameterType> type
    )
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        std::optional<SParameter> authored;
        const auto* parameter = FindParameterDefinition(name, authored);

        if (!parameter ||
            (type ? *type != parameter->Type : parameter->Type == EParameterType::UInt))
            return false;

        glm::vec4 payload = value;
        switch (parameter->Type)
        {
            case EParameterType::Scalar:
                payload = { value.x, 0.0f, 0.0f, 0.0f };
                break;
            case EParameterType::Vector2:
                payload = { value.x, value.y, 0.0f, 0.0f };
                break;
            case EParameterType::Vector3:
                payload.w = 0.0f;
                break;
            default:
                break;
        }

        auto parameterIndex = UINT32_MAX;
        if (m_CompiledSystem)
        {
            const auto compiled = static_cast<const SCompiledParameter*>(parameter);
            parameterIndex = uint32_t(compiled - m_CompiledSystem->Parameters.data());
        }

        m_ParameterOverrides.insert_or_assign(name, SParameterOverride{
            .Type = parameter->Type,
            .Value = payload,
            .ParameterIndex = parameterIndex
        });

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

        std::optional<SParameter> authored;
        const auto* parameter = FindParameterDefinition(name, authored);

        if (!parameter || parameter->Type == EParameterType::UInt)
            return std::nullopt;

        return ResolveParameterValue(*parameter, m_ParameterOverrides);
    }

    std::optional<uint32_t> SystemInstance::GetParameterUInt(const std::string& name) const
    {
        const std::scoped_lock lock(m_SnapshotMutex);

        std::optional<SParameter> authored;
        const auto* parameter = FindParameterDefinition(name, authored);

        if (!parameter || parameter->Type != EParameterType::UInt)
            return std::nullopt;

        return DecodeUIntParameter(ResolveParameterValue(*parameter, m_ParameterOverrides));
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
                const auto* exposed = FindExposedParameter(*system, entry.first);
                return !exposed || exposed->ParameterIndex >= system->Parameters.size() ||
                    entry.second.Type != system->Parameters[exposed->ParameterIndex].Type;
            }
        );

        for (auto& [name, override] : m_ParameterOverrides)
        {
            const auto* exposed = FindExposedParameter(*system, name);
            override.ParameterIndex = exposed ? exposed->ParameterIndex : UINT32_MAX;
        }

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
            const auto* exposed = FindExposedParameter(*system, entry.first);
            return !exposed || exposed->ParameterIndex >= system->Parameters.size() ||
                entry.second.Type != system->Parameters[exposed->ParameterIndex].Type;
        });

        for (auto& [name, override] : m_ParameterOverrides)
        {
            const auto* exposed = FindExposedParameter(*system, name);
            override.ParameterIndex = exposed ? exposed->ParameterIndex : UINT32_MAX;
        }

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
            ResolveParameterWords(*m_CompiledSystem, m_ParameterOverrides)
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
        const SParameter& parameter,
        const ParameterOverridesMap& overrides
    )
    {
        const auto found = overrides.find(parameter.Name);
        return found != overrides.end() && found->second.Type == parameter.Type
            ? found->second.Value
            : parameter.Value;
    }

    Ref<const ResolvedParameterWords> SystemInstance::ResolveParameterWords(
        const SCompiledSystem& system,
        const ParameterOverridesMap& overrides
    )
    {
        auto words = CreateRef<ResolvedParameterWords>(system.DefaultParameterWords);
        for (const auto& override : overrides | std::views::values)
        {
            const auto index = override.ParameterIndex;
            if (index >= system.Parameters.size())
                continue;

            const auto& parameter = system.Parameters[index];
            const auto count = GetParameterWordCount(parameter.Type);

            if (override.Type != parameter.Type || count == 0 ||
                std::size_t(parameter.WordOffset) + count > words->size())
                continue;

            std::memcpy(
                words->data() + parameter.WordOffset,
                &override.Value.x,
                count * sizeof(uint32_t)
            );
        }

        return words;
    }
}
