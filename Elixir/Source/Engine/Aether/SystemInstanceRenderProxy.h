#pragma once

#include <Engine/Aether/SystemInstance.h>

namespace Elixir::Aether
{
    // Renderer-facing state resolved from one immutable instance snapshot.
    class SystemInstanceRenderProxy
    {
        friend class SystemInstanceSnapshot;
        friend class FrameSubmission;
        friend class Renderer;

    public:
        uint32_t GetRevision() const { return m_Revision; }
        uint32_t GetParameterRevision() const { return m_ParameterRevision; }
        const SCompiledSystem& GetCompiledSystem() const { return *m_CompiledSystem; }
        const glm::mat4& GetWorldTransform() const { return m_WorldTransform; }

        glm::vec4 GetParameterValue(const uint32_t parameterIndex) const
        {
            EE_CORE_ASSERT(
                parameterIndex < m_ParameterValues.size(),
                "Aether parameter index is outside the render proxy table."
            )
            return parameterIndex < m_ParameterValues.size()
                ? m_ParameterValues[parameterIndex]
                : glm::vec4{};
        }

    private:
        SystemInstanceRenderProxy(
            const SSystemInstanceKey& key,
            uint32_t revision,
            uint32_t parameterRevision,
            Ref<const SCompiledSystem> system,
            const glm::mat4& worldTransform,
            const ParameterOverridesMap& overrides
        );

        const SSystemInstanceKey& GetKey() const { return m_Key; }

        SSystemInstanceKey m_Key;
        uint32_t m_Revision = 1;
        uint32_t m_ParameterRevision = 1;
        Ref<const SCompiledSystem> m_CompiledSystem;
        glm::mat4 m_WorldTransform{ 1.0f };
        std::vector<glm::vec4> m_ParameterValues;
    };
}
