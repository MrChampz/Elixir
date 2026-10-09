#pragma once

#include <map>
#include <cstring>

namespace Elixir::Aether::Core
{
    /** @brief Identifies the logical type of an authored parameter. */
    enum class EParameterType : uint8_t
    {
        /** @brief Allows scalar or vector reads of low-level compiled float4 data. */
        Unspecified,

        /** @brief Uses one floating-point component. */
        Float,

        /** @brief Uses four floating-point components. */
        Float4,

        /** @brief Uses two floating-point components. */
        Float2,

        /** @brief Uses three floating-point components. */
        Float3,

        /** @brief Uses one unsigned 32-bit integer. */
        UInt
    };

    /** @brief Stores one named parameter and its declared authoring type. */
    struct SParameter
    {
        /** @brief Name used to bind defaults and instance overrides. */
        std::string Name;

        /** @brief Declared type, or Unspecified for low-level compiled values. */
        EParameterType Type = EParameterType::Unspecified;

        /** @brief CPU default; UInt stores its bits in x and unused components are zero. */
        glm::vec4 Value{};
    };

    /** @brief Returns the native word count, or zero for an unsupported type. */
    constexpr uint32_t GetParameterWordCount(const EParameterType type)
    {
        switch (type)
        {
            case EParameterType::Float:
            case EParameterType::UInt:          return 1;
            case EParameterType::Float2:        return 2;
            case EParameterType::Float3:        return 3;
            case EParameterType::Float4:
            case EParameterType::Unspecified:   return 4;
        }

        return 0;
    }

    /** @brief Packs an unsigned integer into a float4 slot without a numeric conversion. */
    inline glm::vec4 EncodeUIntParameter(const uint32_t value)
    {
        glm::vec4 payload{};
        std::memcpy(&payload.x, &value, sizeof(value));
        return payload;
    }

    /** @brief Reads an unsigned integer's bits from the first component of a float4 slot. */
    inline uint32_t DecodeUIntParameter(const glm::vec4& payload)
    {
        uint32_t value = 0;
        std::memcpy(&value, &payload.x, sizeof(value));
        return value;
    }

    /** @brief Stores named defaults with one logical type per name. */
    class ELIXIR_API ParameterStore
    {
      public:
        /** @brief Returns a scalar default, or the fallback when its name is absent. */
        float GetFloat(const std::string& name, float fallback = 0.0f) const;

        /** @brief Sets a scalar default, replacing any other type with the same name. */
        void SetFloat(const std::string& name, float value);

        /** @brief Returns a four-component default, or the fallback when the name is absent or has another type. */
        glm::vec4 GetFloat4(const std::string& name, const glm::vec4& fallback = glm::vec4()) const;

        /** @brief Sets a four-component default, replacing any other type with the same name. */
        void SetFloat4(const std::string& name, const glm::vec4& value);

        /** @brief Returns a two-component default, or the fallback when absent. */
        glm::vec2 GetFloat2(const std::string& name, const glm::vec2& fallback = {}) const;

        /** @brief Sets a two-component default, replacing any other type with the same name. */
        void SetFloat2(const std::string& name, const glm::vec2& value);

        /** @brief Returns a three-component default, or the fallback when absent. */
        glm::vec3 GetFloat3(const std::string& name, const glm::vec3& fallback = {}) const;

        /** @brief Sets a three-component default, replacing any other type with the same name. */
        void SetFloat3(const std::string& name, const glm::vec3& value);

        /** @brief Returns an unsigned integer default, or the fallback when absent. */
        uint32_t GetUInt(const std::string& name, uint32_t fallback = 0) const;

        /** @brief Sets an unsigned integer default without converting it to float. */
        void SetUInt(const std::string& name, uint32_t value);

        /** @brief Compiles typed defaults with the optional scope prefix. */
        std::vector<SParameter> Compile(const std::string& prefix = "") const;

      private:
        // Removes an existing name before assigning its replacement type.
        void Erase(const std::string& name);

        std::map<std::string, float> m_Float;
        std::map<std::string, glm::vec4> m_Float4;
        std::map<std::string, glm::vec2> m_Float2;
        std::map<std::string, glm::vec3> m_Float3;
        std::map<std::string, uint32_t> m_UInt;
    };

    /** @brief Finds a named parameter, or returns UINT32_MAX when absent. */
    template<typename Parameter>
    uint32_t FindParameterIndex(
        const std::vector<Parameter>& parameters,
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
    template<typename Parameter>
    uint32_t FindScopedParameterIndex(
        const std::vector<Parameter>& parameters,
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
