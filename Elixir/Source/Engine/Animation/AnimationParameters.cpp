#include "epch.h"
#include "AnimationParameters.h"

namespace Elixir
{
    /* SAnimationCondition */

    bool SAnimationCondition::IsValid() const
    {
        if (Parameter.empty()) return false;

        const auto* numeric = std::get_if<float>(&Value);
        if (numeric && !std::isfinite(*numeric)) return false;

        return Comparison == EAnimationComparison::Equal ||
            Comparison == EAnimationComparison::NotEqual ||
            (numeric && (Comparison == EAnimationComparison::Less ||
                Comparison == EAnimationComparison::LessEqual ||
                Comparison == EAnimationComparison::Greater ||
                Comparison == EAnimationComparison::GreaterEqual));
    }

    bool SAnimationCondition::IsValid(const AnimationParameters& parameters) const
    {
        if (!IsValid()) return false;

        const auto index = parameters.Find(Parameter);
        const auto* parameter = index
            ? parameters.Get(*index)
            : nullptr;

        return parameter && parameter->index() == Value.index();
    }

    /* AnimationParameters */

    AnimationParameters::AnimationParameters(std::vector<SAnimationParameter> definitions)
        : m_Definitions(std::move(definitions))
    {
        Reset();
    }

    void AnimationParameters::Reset()
    {
        m_Values.clear();
        m_Values.reserve(m_Definitions.size());

        for (const auto& definition : m_Definitions)
            m_Values.push_back(definition.DefaultValue);
    }

    bool AnimationParameters::IsValid() const
    {
        std::unordered_set<std::string> names;

        for (const auto& definition : m_Definitions)
        {
            if (definition.Name.empty()) return false;

            const auto [_, inserted] = names.insert(definition.Name);
            if (!inserted) return false;

            if (const auto* value = std::get_if<float>(&definition.DefaultValue))
                if (!std::isfinite(*value)) return false;
        }

        return true;
    }

    std::optional<size_t> AnimationParameters::Find(const std::string_view name) const
    {
        for (size_t i = 0; i < m_Definitions.size(); ++i)
            if (m_Definitions[i].Name == name)
                return i;

        return std::nullopt;
    }

    bool AnimationParameters::SetFloat(const std::string_view name, const float value)
    {
        const auto index = Find(name);

        if (!index || !std::holds_alternative<float>(m_Values[*index]) || !std::isfinite(value))
            return false;

        m_Values[*index] = value;
        return true;
    }

    bool AnimationParameters::SetBool(const std::string_view name, const bool value)
    {
        const auto index = Find(name);

        if (!index || !std::holds_alternative<bool>(m_Values[*index]))
            return false;

        m_Values[*index] = value;
        return true;
    }

    std::optional<float> AnimationParameters::GetFloat(const std::string_view name) const
    {
        const auto index = Find(name);
        if (!index) return std::nullopt;

        const auto* value = std::get_if<float>(&m_Values[*index]);
        return value ? std::optional(*value) : std::nullopt;
    }

    std::optional<bool> AnimationParameters::GetBool(const std::string_view name) const
    {
        const auto index = Find(name);
        if (!index) return std::nullopt;

        const auto* value = std::get_if<bool>(&m_Values[*index]);
        return value ? std::optional(*value) : std::nullopt;
    }

    const TAnimationParameterValue* AnimationParameters::Get(const size_t index) const
    {
        return index < m_Values.size() ? &m_Values[index] : nullptr;
    }

    bool AnimationParameters::Matches(const SAnimationCondition& condition) const
    {
        const auto index = Find(condition.Parameter);
        return index && Matches(*index, condition);
    }

    bool AnimationParameters::Matches(
        const size_t index,
        const SAnimationCondition& condition
    ) const
    {
        const auto* parameter = Get(index);

        if (!parameter || parameter->index() != condition.Value.index())
            return false;

        if (const auto* value = std::get_if<bool>(parameter))
        {
            const auto equal = *value == std::get<bool>(condition.Value);

            if (condition.Comparison == EAnimationComparison::Equal)
                return equal;

            return condition.Comparison == EAnimationComparison::NotEqual && !equal;
        }

        const auto value = std::get<float>(*parameter);
        const auto target = std::get<float>(condition.Value);

        switch (condition.Comparison)
        {
            case EAnimationComparison::Equal:           return value == target;
            case EAnimationComparison::NotEqual:        return value != target;
            case EAnimationComparison::Less:            return value < target;
            case EAnimationComparison::LessEqual:       return value <= target;
            case EAnimationComparison::Greater:         return value > target;
            case EAnimationComparison::GreaterEqual:    return value >= target;
        }

        return false;
    }
}
