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
        /** Name used to bind defaults and instance overrides. */
        std::string Name;

        /** Declared type, or Unspecified for low-level compiled values. */
        EParameterType Type = EParameterType::Unspecified;

        /** CPU default; UInt stores its bits in x and unused components are zero. */
        glm::vec4 Value{};
    };

    /**
     * @brief Get the native storage width of a parameter.
     * @param type Declared parameter type.
     * @return Number of four-byte words, or zero for an unsupported type.
     */
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

    /**
     * @brief Pack an unsigned integer without numeric conversion.
     * @param value Unsigned value to encode.
     * @return Payload with the raw bits in x and zero in the remaining components.
     */
    inline glm::vec4 EncodeUIntParameter(const uint32_t value)
    {
        glm::vec4 payload{};
        std::memcpy(&payload.x, &value, sizeof(value));
        return payload;
    }

    /**
     * @brief Read an unsigned integer from a float payload.
     * @param payload Payload whose x component contains the raw integer bits.
     */
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
        /**
         * @brief Get a scalar default.
         * @param name Parameter name.
         * @param fallback Value used when the name is absent or has another type.
         * @return The stored value, or fallback.
         */
        float GetFloat(const std::string& name, float fallback = 0.0f) const;

        /**
         * @brief Set a scalar default, replacing any type with the same name.
         * @param name Parameter name.
         * @param value Default value to store.
         */
        void SetFloat(const std::string& name, float value);

        /**
         * @brief Get a four-component default.
         * @param name Parameter name.
         * @param fallback Value used when the name is absent or has another type.
         * @return The stored value, or fallback.
         */
        glm::vec4 GetFloat4(const std::string& name, const glm::vec4& fallback = glm::vec4()) const;

        /**
         * @brief Set a four-component default, replacing any type with the same name.
         * @param name Parameter name.
         * @param value Default value to store.
         */
        void SetFloat4(const std::string& name, const glm::vec4& value);

        /**
         * @brief Get a two-component default.
         * @param name Parameter name.
         * @param fallback Value used when the name is absent or has another type.
         * @return The stored value, or fallback.
         */
        glm::vec2 GetFloat2(const std::string& name, const glm::vec2& fallback = {}) const;

        /**
         * @brief Set a two-component default, replacing any type with the same name.
         * @param name Parameter name.
         * @param value Default value to store.
         */
        void SetFloat2(const std::string& name, const glm::vec2& value);

        /**
         * @brief Get a three-component default.
         * @param name Parameter name.
         * @param fallback Value used when the name is absent or has another type.
         * @return The stored value, or fallback.
         */
        glm::vec3 GetFloat3(const std::string& name, const glm::vec3& fallback = {}) const;

        /**
         * @brief Set a three-component default, replacing any type with the same name.
         * @param name Parameter name.
         * @param value Default value to store.
         */
        void SetFloat3(const std::string& name, const glm::vec3& value);

        /**
         * @brief Get an unsigned integer default.
         * @param name Parameter name.
         * @param fallback Value used when the name is absent or has another type.
         * @return The stored value, or fallback.
         */
        uint32_t GetUInt(const std::string& name, uint32_t fallback = 0) const;

        /**
         * @brief Set an unsigned integer default, replacing any type with the same name.
         * @param name Parameter name.
         * @param value Default value to store.
         */
        void SetUInt(const std::string& name, uint32_t value);

        /**
         * @brief Compile typed defaults with an optional scope prefix.
         * @param prefix Text prepended to each name; include a scope separator when needed.
         */
        std::vector<SParameter> Compile(const std::string& prefix = "") const;

      private:
        /**
         * @brief Remove an existing name before replacing its type.
         * @param name Parameter name to remove from all type tables.
         */
        void Erase(const std::string& name);

        std::map<std::string, float> m_Float;
        std::map<std::string, glm::vec4> m_Float4;
        std::map<std::string, glm::vec2> m_Float2;
        std::map<std::string, glm::vec3> m_Float3;
        std::map<std::string, uint32_t> m_UInt;
    };

    /**
     * @brief Find a named parameter.
     * @tparam Parameter Table element type with a Name member.
     * @param parameters Parameter table to search.
     * @param name Exact parameter name.
     * @return The first matching index, or UINT32_MAX when absent.
     */
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

    /**
     * @brief Find an emitter-local parameter before checking the system scope.
     * @tparam Parameter Table element type with a Name member.
     * @param parameters Parameter table to search.
     * @param emitterName Emitter name used as the local scope prefix.
     * @param parameterName Unscoped parameter name; empty names are not resolved.
     * @return The first matching index, or UINT32_MAX when absent.
     */
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
