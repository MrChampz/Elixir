#pragma once

#include <map>

namespace Elixir::Aether::Core
{
    /** @brief Identifies authored scalar or vector parameter storage. */
    enum class EParameterType : uint8_t
    {
        Unspecified, Scalar, Vector
    };

    /** @brief Stores one named parameter and its declared authoring type. */
    struct SParameter
    {
        std::string Name;

        /** @brief Declared type, or Unspecified for low-level compiled values. */
        EParameterType Type = EParameterType::Unspecified;

        /** @brief Default value; scalar parameters use the x component. */
        glm::vec4 Value{};
    };

    /** @brief Stores named scalar and vector defaults with one type per name. */
    class ELIXIR_API ParameterStore
    {
      public:
        /** @brief Returns a scalar default, or the fallback when its name is absent. */
        float GetFloat(const std::string& name, float fallback = 0.0f) const;

        /** @brief Sets a scalar default, replacing any vector with the same name. */
        void SetFloat(const std::string& name, float value);

        /** @brief Returns a vector default, or the fallback when its name is absent. */
        glm::vec4 GetFloat4(const std::string& name, const glm::vec4& fallback = glm::vec4()) const;

        /** @brief Sets a vector default, replacing any scalar with the same name. */
        void SetFloat4(const std::string& name, const glm::vec4& value);

        /** @brief Compiles typed defaults with the optional scope prefix. */
        std::vector<SParameter> Compile(const std::string& prefix = "") const;

      private:
        std::map<std::string, float> m_Float;
        std::map<std::string, glm::vec4> m_Float4;
    };

    /** @brief Finds a named parameter, or returns UINT32_MAX when absent. */
    inline uint32_t FindParameterIndex(
        const std::vector<SParameter>& parameters,
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

    /** @brief Finds an emitter-local parameter before checking the system scope. */
    inline uint32_t FindScopedParameterIndex(
        const std::vector<SParameter>& parameters,
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
