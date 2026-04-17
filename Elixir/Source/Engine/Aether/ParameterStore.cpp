#include "epch.h"
#include "ParameterStore.h"

namespace Elixir::Aether
{
    float ParameterStore::GetFloat(const std::string& name, const float fallback) const
    {
        const auto found = m_Float.find(name);
        return found != m_Float.end() ? found->second : fallback;
    }

    void ParameterStore::SetFloat(const std::string& name, const float value)
    {
        m_Float[name] = value;
    }

    glm::vec4 ParameterStore::GetFloat4(
        const std::string& name,
        const glm::vec4& fallback
    ) const
    {
        const auto found = m_Float4.find(name);
        return found != m_Float4.end() ? found->second : fallback;
    }

    void ParameterStore::SetFloat4(const std::string& name, const glm::vec4& value)
    {
        m_Float4[name] = value;
    }

    std::vector<SGPUParameter> ParameterStore::Compile() const
    {
        std::vector<SGPUParameter> parameters;
        parameters.reserve(m_Float.size() + m_Float4.size());

        for (const auto& [name, value] : m_Float)
            parameters.push_back({ name, { value, 0.0f, 0.0f, 0.0f } });

        for (const auto& [name, value] : m_Float4)
            parameters.push_back({ name, value });

        return parameters;
    }
}