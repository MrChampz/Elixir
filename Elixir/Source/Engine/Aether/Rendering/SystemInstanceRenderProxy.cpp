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
        Ref<const ResolvedParameterValues> parameters
    ) : m_Key(key),
        m_Revision(revision),
        m_ParameterRevision(parameterRevision),
        m_CompiledSystem(std::move(system)),
        m_WorldTransform(worldTransform),
        m_Parameters(std::move(parameters))
    {
        EE_CORE_ASSERT(
            m_Parameters,
            "SystemInstanceRenderProxy requires resolved parameter values."
        )
        EE_CORE_ASSERT(
            m_Parameters->size() == m_CompiledSystem->Parameters.size(),
            "Aether resolved parameter table does not match the compiled system."
        )
    }
}
