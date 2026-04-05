#pragma once

#include <map>

namespace Elixir::Aether
{
    class ELIXIR_API ParameterStore
    {
      public:
        float GetFloat(const std::string& name, const float fallback = 0.0f) const
        {
            const auto found = m_Floats.find(name);
            return found != m_Floats.end() ? found->second : fallback;
        }

        void SetFloat(const std::string& name, const float value)
        {
            m_Floats[name] = value;
        }

      private:
        std::map<std::string, float> m_Floats;
    };
}