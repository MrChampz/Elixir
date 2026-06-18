#pragma once

#include <map>

namespace Elixir::Aether
{
    struct SGPUParameter
    {
        std::string Name;
        glm::vec4 Value{};
    };

    class ELIXIR_API ParameterStore
    {
      public:
        float GetFloat(const std::string& name, float fallback = 0.0f) const;
        void SetFloat(const std::string& name, float value);

        glm::vec4 GetFloat4(const std::string& name, const glm::vec4& fallback = glm::vec4()) const;
        void SetFloat4(const std::string& name, const glm::vec4& value);

        std::vector<SGPUParameter> Compile(const std::string& prefix = "") const;

      private:
        std::map<std::string, float> m_Float;
        std::map<std::string, glm::vec4> m_Float4;
    };

    inline uint32_t FindParameterIndex(
        const std::vector<SGPUParameter>& parameters,
        const std::string& name
    )
    {
        for (uint32_t i = 0; i < parameters.size(); ++i)
        {
            if (parameters[i].Name == name)
                return i;
        }
        return UINT32_MAX;
    }

    inline uint32_t FindScopedParameterIndex(
        const std::vector<SGPUParameter>& parameters,
        const std::string& emitterName,
        const std::string& parameterName
    )
    {
        if (parameterName.empty())
            return UINT32_MAX;

        const uint32_t localIndex = FindParameterIndex(parameters, emitterName + "." + parameterName);
        if (localIndex != UINT32_MAX)
            return localIndex;

        return FindParameterIndex(parameters, parameterName);
    }
}