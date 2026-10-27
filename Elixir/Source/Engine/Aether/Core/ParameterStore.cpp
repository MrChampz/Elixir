#include "epch.h"
#include "ParameterStore.h"

namespace Elixir::Aether::Core
{
    float ParameterStore::GetFloat(const std::string& name, const float fallback) const
    {
        const auto found = m_Float.find(name);
        return found != m_Float.end() ? found->second : fallback;
    }

    void ParameterStore::SetFloat(const std::string& name, const float value)
    {
        Erase(name);
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
        Erase(name);
        m_Float4[name] = value;
    }

    glm::vec2 ParameterStore::GetFloat2(const std::string& name, const glm::vec2& fallback) const
    {
        const auto found = m_Float2.find(name);
        return found != m_Float2.end() ? found->second : fallback;
    }

    void ParameterStore::SetFloat2(const std::string& name, const glm::vec2& value)
    {
        Erase(name);
        m_Float2[name] = value;
    }

    glm::vec3 ParameterStore::GetFloat3(const std::string& name, const glm::vec3& fallback) const
    {
        const auto found = m_Float3.find(name);
        return found != m_Float3.end() ? found->second : fallback;
    }

    void ParameterStore::SetFloat3(const std::string& name, const glm::vec3& value)
    {
        Erase(name);
        m_Float3[name] = value;
    }

    uint32_t ParameterStore::GetUInt(const std::string& name, const uint32_t fallback) const
    {
        const auto found = m_UInt.find(name);
        return found != m_UInt.end() ? found->second : fallback;
    }

    void ParameterStore::SetUInt(const std::string& name, const uint32_t value)
    {
        Erase(name);
        m_UInt[name] = value;
    }

    void ParameterStore::Erase(const std::string& name)
    {
        m_Float.erase(name);
        m_Float4.erase(name);
        m_Float2.erase(name);
        m_Float3.erase(name);
        m_UInt.erase(name);
    }

    std::vector<SParameter> ParameterStore::Compile(const std::string& prefix) const
    {
        std::vector<SParameter> parameters;
        parameters.reserve(m_Float.size() + m_Float4.size() + m_Float2.size() + m_Float3.size() + m_UInt.size());

        for (const auto& [name, value] : m_Float)
            parameters.push_back({
                .Name = prefix + name,
                .Type = EParameterType::Float,
                .Value = { value, 0.0f, 0.0f, 0.0f }
            });

        for (const auto& [name, value] : m_Float4)
            parameters.push_back({
                .Name = prefix + name,
                .Type = EParameterType::Float4,
                .Value = value
            });

        for (const auto& [name, value] : m_Float2)
            parameters.push_back({
                .Name = prefix + name,
                .Type = EParameterType::Float2,
                .Value = { value, 0.0f, 0.0f }
            });

        for (const auto& [name, value] : m_Float3)
            parameters.push_back({
                .Name = prefix + name,
                .Type = EParameterType::Float3,
                .Value = { value, 0.0f }
            });

        for (const auto& [name, value] : m_UInt)
            parameters.push_back({
                .Name = prefix + name,
                .Type = EParameterType::UInt,
                .Value = EncodeUIntParameter(value)
            });

        return parameters;
    }
}
