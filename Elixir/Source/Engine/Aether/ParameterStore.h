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
        float GetFloat(const std::string& name, const float fallback = 0.0f) const;
        void SetFloat(const std::string& name, const float value);

        glm::vec4 GetFloat4(const std::string& name, const glm::vec4& fallback = glm::vec4()) const;
        void SetFloat4(const std::string& name, const glm::vec4& value);

        std::vector<SGPUParameter> Compile() const;

      private:
        std::map<std::string, float> m_Float;
        std::map<std::string, glm::vec4> m_Float4;
    };
}