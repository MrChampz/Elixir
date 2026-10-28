#pragma once

#include <Engine/Core/Core.h>

namespace Elixir
{
    /** Stores a numeric or boolean animation parameter. */
    using TAnimationParameterValue = std::variant<float, bool>;

    /** Selects how a transition compares a parameter with its condition value. */
    enum class EAnimationComparison : uint8_t
    {
        Equal,
        NotEqual,
        Less,
        LessEqual,
        Greater,
        GreaterEqual
    };

    /** Defines a parameter and its initial value for each animation. */
    struct SAnimationParameter
    {
        std::string Name;
        TAnimationParameterValue DefaultValue = 0.0f;
    };

    /** Compares one parameter with a value of the same type. */
    struct ELIXIR_API SAnimationCondition
    {
        std::string Parameter;
        EAnimationComparison Comparison = EAnimationComparison::Equal;
        TAnimationParameterValue Value = 0.0f;

        /** @brief Check intrinsic name, comparison, and finite value requirements. */
        bool IsValid() const;

        /**
         * @brief Validate the parameter type, comparison, and finite condition value.
         * @param parameters Character parameter layout.
         */
        bool IsValid(const class AnimationParameters& parameters) const;
    };

    /** Typed values and their immutable name layout for one character. */
    class ELIXIR_API AnimationParameters
    {
    public:
        /**
         * @brief Initialize a parameter layout and its default values.
         * @param definitions Named typed defaults to retain.
         */
        explicit AnimationParameters(std::vector<SAnimationParameter> definitions = {});

        /** @brief Check that definitions have unique names and finite values. */
        bool IsValid() const;

        /**
         * @brief Find a resolved parameter index.
         * @param name Parameter name.
         */
        std::optional<size_t> Find(std::string_view name) const;

        /**
         * @brief Replace an existing finite numeric value without changing its type.
         * @param name Numeric parameter name.
         * @param value Finite replacement value.
         */
        bool SetFloat(std::string_view name, float value);

        /**
         * @brief Replace an existing boolean value.
         * @param name Boolean parameter name.
         * @param value Replacement value.
         */
        bool SetBool(std::string_view name, bool value);

        /**
         * @brief Read a numeric value, or return empty for a type or name mismatch.
         * @param name Numeric parameter name.
         */
        std::optional<float> GetFloat(std::string_view name) const;

        /**
         * @brief Read a boolean value, or return empty for a type or name mismatch.
         * @param name Boolean parameter name.
         */
        std::optional<bool> GetBool(std::string_view name) const;

        /**
         * @brief Inspect a value by its resolved index.
         * @param index Parameter index established during binding.
         */
        const TAnimationParameterValue* Get(size_t index) const;

        /**
         * @brief Evaluate a typed condition without converting values.
         * @param condition Condition to compare against this instance.
         */
        bool Matches(const SAnimationCondition& condition) const;

        /**
         * @brief Evaluate a condition using its previously resolved parameter index.
         * @param index Parameter index established during source binding.
         * @param condition Typed comparison to evaluate.
         */
        bool Matches(size_t index, const SAnimationCondition& condition) const;

        /** @brief Restore every value to its authored default. */
        void Reset();

        /** @brief Get the immutable parameter definitions. */
        const std::vector<SAnimationParameter>& GetDefinitions() const { return m_Definitions; }

    private:
        std::vector<SAnimationParameter> m_Definitions;
        std::vector<TAnimationParameterValue> m_Values;
    };
}
