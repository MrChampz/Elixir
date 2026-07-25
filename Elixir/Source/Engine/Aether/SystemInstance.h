#pragma once

#include <Engine/Aether/System.h>

namespace Elixir::Aether
{
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

        uint32_t GetRevision() const { return m_Revision; }
        uint32_t GetParameterRevision() const { return m_ParameterRevision; }

        const SCompiledSystem& GetCompiledSystem() const { return *m_CompiledSystem; }
        void SetCompiledSystem(Ref<const SCompiledSystem> compiledSystem);

        const glm::mat4& GetWorldTransform() const { return m_WorldTransform; }
        void SetWorldTransform(const glm::mat4& worldTransform);

        bool SetParameterOverride(std::string name, const glm::vec4& value);
        bool ClearParameterOverride(const std::string& name);
        void ClearParameterOverrides();

        glm::vec4 ResolveParameterValue(uint32_t parameterIndex) const;

    private:
        bool IsExposedParameter(const std::string& name) const;

        UUID m_Id;
        uint32_t m_Revision = 1;
        uint32_t m_ParameterRevision = 1;
        Ref<const SCompiledSystem> m_CompiledSystem;

        glm::mat4 m_WorldTransform{ 1.0f };
        std::unordered_map<std::string, glm::vec4> m_ParameterOverrides;
    };
}
