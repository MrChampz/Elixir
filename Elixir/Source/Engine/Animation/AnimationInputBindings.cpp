#include "epch.h"
#include "AnimationInputBindings.h"

namespace Elixir
{
    bool AnimationInputBindings::Bind(
        Ref<AnimationParameters> parameters,
        const std::span<const size_t> bindings
    )
    {
        if (!parameters || !parameters->IsValid())
            return false;

        AnimationInputBindings candidate = *this;
        candidate.m_Parameters = std::move(parameters);
        candidate.m_Indices.assign(bindings.begin(), bindings.end());

        if (!candidate.Refresh())
            return false;

        *this = std::move(candidate);
        return true;
    }

    bool AnimationInputBindings::SetValues(const std::span<const float> values)
    {
        for (const auto value : values)
            if (!std::isfinite(value))
                return false;

        m_Values.assign(values.begin(), values.end());
        return true;
    }

    bool AnimationInputBindings::Refresh()
    {
        if (m_Indices.empty()) return true;

        if (!m_Parameters) return false;

        for (const auto index : m_Indices)
        {
            const auto* value = m_Parameters->Get(index);
            const auto* numeric = value
                ? std::get_if<float>(value)
                : nullptr;

            if (!numeric || !std::isfinite(*numeric))
                return false;
        }

        m_Values.resize(m_Indices.size());

        for (size_t i = 0; i < m_Indices.size(); ++i)
            m_Values[i] = std::get<float>(*m_Parameters->Get(m_Indices[i]));

        return true;
    }
}
