#include "epch.h"
#include "SystemInstance.h"

namespace Elixir::Aether
{
    SystemInstance::SystemInstance(Ref<const SCompiledSystem> compiledSystem)
        : m_CompiledSystem(std::move(compiledSystem))
    {
        EE_CORE_ASSERT(m_CompiledSystem, "SystemInstance requires a compiled system.")
    }

    void SystemInstance::SetCompiledSystem(Ref<const SCompiledSystem> compiledSystem)
    {
        EE_CORE_ASSERT(compiledSystem, "SystemInstance requires a compiled system.")

        if (m_CompiledSystem == compiledSystem)
            return;

        m_CompiledSystem = std::move(compiledSystem);

        bool removedOverrides = false;

        for (auto it = m_ParameterOverrides.begin(); it != m_ParameterOverrides.end();)
        {
            if (IsExposedParameter(it->first))
            {
                 ++it;
                continue;
            }
            it = m_ParameterOverrides.erase(it);
            removedOverrides = true;
        }

        ++m_Revision;

        if (removedOverrides)
            ++m_ParameterRevision;
    }

    void SystemInstance::SetWorldTransform(const glm::mat4& worldTransform)
    {
        m_WorldTransform = worldTransform;
    }

    bool SystemInstance::SetParameterOverride(std::string name, const glm::vec4& value)
    {
        if (!IsExposedParameter(name))
            return false;

        m_ParameterOverrides.insert_or_assign(std::move(name), value);
        ++m_ParameterRevision;

        return true;
    }

    bool SystemInstance::ClearParameterOverride(const std::string& name)
    {
        const auto found = m_ParameterOverrides.find(name);
        if (found == m_ParameterOverrides.end())
            return false;

        m_ParameterOverrides.erase(found);
        ++m_ParameterRevision;

        return true;
    }

    void SystemInstance::ClearParameterOverrides()
    {
        if (m_ParameterOverrides.empty())
            return;

        m_ParameterOverrides.clear();
        ++m_ParameterRevision;
    }

    glm::vec4 SystemInstance::ResolveParameterValue(uint32_t parameterIndex) const
    {
        EE_CORE_ASSERT(
            parameterIndex < m_CompiledSystem->Parameters.size(),
            "Aether parameter index is outside the compiled system parameter table."
        )

        if (parameterIndex >= m_CompiledSystem->Parameters.size())
            return {};

        const auto& parameter = m_CompiledSystem->Parameters[parameterIndex];
        const auto found = m_ParameterOverrides.find(parameter.Name);

        return found != m_ParameterOverrides.end()
            ? found->second
            : parameter.Value;
    }

    bool SystemInstance::IsExposedParameter(const std::string& name) const
    {
        return std::ranges::any_of(
            m_CompiledSystem->ExposedParameters,
            [&name](const SExposedParameter& parameter)
            {
                return parameter.Name == name;
            }
        );
    }
}
