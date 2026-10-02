#pragma once

#include <cmath>
#include <format>
#include <limits>
#include <optional>

#include <magic_enum/magic_enum.hpp>
#include <simdjson.h>

#include <Engine/Aether/Modules/DynamicInput.h>
#include <Engine/Aether/Modules/Module.h>

namespace Elixir::Aether::Effect
{
    /** @brief Stores a scalar literal or a named parameter reference. */
    struct ScalarField
    {
        /** @brief Literal value used when no parameter is bound. */
        float Value = 0.0f;

        /** @brief Parameter name without the dollar prefix, or empty for a literal. */
        std::string Param;
    };

    /** @brief Stores a vector literal or a named parameter reference. */
    struct Float4Field
    {
        /** @brief Literal value used when no parameter is bound. */
        glm::vec4 Value{};

        /** @brief Parameter name without the dollar prefix, or empty for a literal. */
        std::string Param;
    };

    /** @brief Reads effect fields and preserves the first parsing error. */
    class ModuleParseContext
    {
    public:
        /** @brief Reports whether any field failed validation. */
        bool Failed() const { return m_Failed; }

        /** @brief Returns the first validation error, or an empty string. */
        const std::string& GetError() const { return m_Error; }

        /** @brief Records the first validation error and stops subsequent reads. */
        template <typename... Args>
        void Fail(const std::string_view message, Args&&... args)
        {
            if (m_Failed) return;
            m_Error = std::vformat(message, std::make_format_args(args...));
            m_Failed = true;
        }

        /* Scalar accessors */

        /** @brief Reads a required numeric field. */
        float RequireFloat(simdjson::ondemand::object& object, std::string_view key)
        {
            if (Failed()) return 0.0f;

            auto field = object[key];
            double value;

            if (field.error() || field.get_double().get(value))
            {
                Fail("Field '{}' must be a number.", key);
                return 0.0f;
            }

            return ToFiniteFloat(value, key);
        }

        /** @brief Reads a required integer within the uint32 range. */
        uint32_t RequireUInt(simdjson::ondemand::object& object, std::string_view key)
        {
            if (Failed()) return 0;

            auto field = object[key];
            uint64_t value;

            if (field.error() || field.get_uint64().get(value))
            {
                Fail("Field '{}' must be an unsigned integer.", key);
                return 0;
            }

            if (value > std::numeric_limits<uint32_t>::max())
            {
                Fail("Field '{}' exceeds uint32 range.", key);
                return 0;
            }

            return static_cast<uint32_t>(value);
        }

        /** @brief Reads a required string field. */
        std::string RequireString(simdjson::ondemand::object& object, std::string_view key)
        {
            if (Failed()) return {};

            auto field = object[key];
            std::string_view value;

            if (field.error() || field.get_string().get(value))
            {
                Fail("Field '{}' must be a string.", key);
                return {};
            }

            return std::string{ value };
        }

        /** @brief Reports whether the object contains a field. */
        static bool HasField(simdjson::ondemand::object& object, std::string_view key)
        {
            return !object[key].error();
        }

        /* Vector accessors */

        /** @brief Reads a required numeric array with N elements. */
        template <int N>
        glm::vec<N, float> RequireFloatVec(simdjson::ondemand::object& object, std::string_view key)
        {
            glm::vec<N, float> result{};
            if (Failed()) return result;

            auto field = object[key];
            simdjson::ondemand::array array;

            if (field.error() || field.get_array().get(array))
            {
                Fail("Field '{}' must be a Float{} array.", key, N);
                return result;
            }

            return ParseFloatVec<N>(array);
        }

        /* Parameter-aware fields */

        /** @brief Reads a number or a parameter reference, using the fallback when absent. */
        ScalarField ParseScalar(
            simdjson::ondemand::object& object,
            std::string_view key,
            const float fallback = 0.0f
        )
        {
            ScalarField result{ fallback, {} };
            if (Failed()) return result;

            auto field = object[key];
            if (field.error())
                return result;

            simdjson::ondemand::json_type type;
            if (field.type().get(type))
            {
                Fail("Field '{}' has an invalid type.", key);
                return result;
            }

            if (type == simdjson::ondemand::json_type::number)
            {
                double value;
                if (field.get_double().get(value))
                {
                    Fail("Field '{}' is not a valid number.", key);
                    return result;
                }

                result.Value = ToFiniteFloat(value, key);
                return result;
            }

            if (type == simdjson::ondemand::json_type::string)
            {
                std::string_view ref;
                if (!field.get_string().get(ref))
                {
                    if (auto param = ResolveParamRef(ref))
                    {
                        result.Param = std::move(*param);
                        return result;
                    }
                }
            }

            Fail("Field '{}' must be a number or a parameter reference.", key);
            return result;
        }

        /** @brief Reads a four-element numeric array or parameter reference, using the fallback when absent. */
        Float4Field ParseFloat4(
            simdjson::ondemand::object& object,
            std::string_view key,
            const glm::vec4 fallback = glm::vec4(0.0f)
        )
        {
            Float4Field result{ fallback, {} };
            if (Failed()) return result;

            auto field = object[key];
            if (field.error())
                return result;

            simdjson::ondemand::json_type type;
            if (field.type().get(type))
            {
                Fail("Field '{}' has an invalid type.", key);
                return result;
            }

            if (type == simdjson::ondemand::json_type::array)
            {
                simdjson::ondemand::array array;
                if (field.get_array().get(array))
                {
                    Fail("Field '{}' is not a valid array.", key);
                    return result;
                }

                result.Value = ParseFloatVec<4>(array);
                return result;
            }

            if (type == simdjson::ondemand::json_type::string)
            {
                std::string_view ref;
                if (!field.get_string().get(ref))
                {
                    if (auto param = ResolveParamRef(ref))
                    {
                        result.Param = std::move(*param);
                        return result;
                    }
                }
            }

            Fail("Field '{}' must be a Float4 array or a parameter reference.", key);
            return result;
        }

        /** @brief Reads a string, using the fallback when absent. */
        std::string ParseString(
            simdjson::ondemand::object& object,
            std::string_view key,
            const std::string_view fallback = ""
        )
        {
            std::string result{ fallback };
            if (Failed()) return result;

            auto field = object[key];
            if (field.error())
                return result;

            simdjson::ondemand::json_type type;
            if (field.type().get(type))
            {
                Fail("Field '{}' has an invalid type.", key);
                return result;
            }

            if (type == simdjson::ondemand::json_type::string)
            {
                std::string_view str;
                if (!field.get_string().get(str))
                {
                    return std::string{ str };
                }
            }

            Fail("Field '{}' must be a string.", key);
            return result;
        }

        /* Enums */

        /** @brief Reads a dynamic input name, using None when absent. */
        Modules::EDynamicInput ParseDynamicInput(simdjson::ondemand::object& object, std::string_view key)
        {
            constexpr auto result = Modules::EDynamicInput::None;

            if (Failed() || !HasField(object, key))
                return result;

            const std::string value = RequireString(object, key);
            if (Failed()) return result;

            if (const auto input = magic_enum::enum_cast<Modules::EDynamicInput>(value))
                return *input;

            Fail("Unknown dynamic input '{}'.", value);
            return result;
        }

        /** @brief Reads an array containing exactly N numeric elements. */
        template <int N>
        glm::vec<N, float> ParseFloatVec(simdjson::ondemand::array array)
        {
            glm::vec<N, float> result{};
            if (Failed()) return result;

            size_t count;
            if (array.count_elements().get(count) || count != N)
            {
                Fail("Expected a Float{} array.", N);
                return result;
            }

            int i = 0;
            for (auto element : array)
            {
                double value;

                if (element.get_double().get(value))
                {
                    Fail("Field array element must be numeric.");
                    return result;
                }

                result[i++] = ToFiniteFloat(value, "array element");
                if (Failed()) return result;
            }

            return result;
        }

    private:
        // Rejects non-finite values and overflow before narrowing to float.
        float ToFiniteFloat(const double value, std::string_view key)
        {
            if (!std::isfinite(value) ||
                std::abs(value) > static_cast<double>(std::numeric_limits<float>::max()))
            {
                Fail("Field '{}' must be a finite float.", key);
                return 0.0f;
            }
            return static_cast<float>(value);
        }

        // Removes the dollar prefix from a parameter reference.
        static std::optional<std::string> ResolveParamRef(std::string_view value)
        {
            if (!value.empty() && value.front() == '$')
                return std::string{ value.substr(1) };
            return std::nullopt;
        }

        bool m_Failed = false;
        std::string m_Error;
    };
}
