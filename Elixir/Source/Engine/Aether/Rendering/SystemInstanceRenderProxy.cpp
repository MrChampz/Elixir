#include "epch.h"
#include "SystemInstanceRenderProxy.h"

namespace Elixir::Aether::Rendering
{
    SystemInstanceRenderProxy::SystemInstanceRenderProxy(
        const SSystemInstanceKey& key,
        const uint32_t revision,
        const uint32_t parameterRevision,
        Ref<const SCompiledSystem> system,
        const glm::mat4& worldTransform,
        const ParameterOverridesMap& overrides
    ) : m_Key(key),
        m_Revision(revision),
        m_ParameterRevision(parameterRevision),
        m_CompiledSystem(std::move(system)),
        m_WorldTransform(worldTransform)
    {
        m_ParameterValues.reserve(m_CompiledSystem->Parameters.size());

        for (const auto& parameter : m_CompiledSystem->Parameters)
        {
            const auto found = overrides.find(parameter.Name);
            const auto value = found != overrides.end() ? found->second : parameter.Value;
            m_ParameterValues.push_back(value);
        }
    }
}
