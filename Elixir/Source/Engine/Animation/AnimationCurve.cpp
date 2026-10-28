#include "epch.h"
#include "AnimationCurve.h"

#include <limits>

namespace Elixir
{
    float AnimationCurveValues::GetValue(const std::string_view name) const
    {
        const auto found = m_Values.find(name);
        return found == m_Values.end() ? 0.0f : found->second;
    }

    bool AnimationCurveValues::SetValue(std::string name, const float value)
    {
        if (name.empty() || !std::isfinite(value))
        {
            EE_CORE_ERROR("Animation curves require a name and a finite scalar value.")
            return false;
        }

        m_Values.insert_or_assign(std::move(name), value);
        return true;
    }

    bool AnimationCurveValues::Blend(const AnimationCurveValues& target, const float weight)
    {
        if (!std::isfinite(weight) || weight < 0 || weight > 1)
        {
            EE_CORE_ERROR("Animation curve blend weight must be in [0, 1].")
            return false;
        }

        AnimationCurveValues result;

        for (const auto& [name, value] : m_Values)
        {
            const auto blended =
                double(value) * (1.0 - weight) + double(target.GetValue(name)) * weight;

            if (!result.SetValue(name, float(blended)))
                return false;
        }

        for (const auto& [name, value] : target.m_Values)
        {
            if (!m_Values.contains(name) &&
                !result.SetValue(name, float(double(value) * weight)))
                return false;
        }

        *this = std::move(result);
        return true;
    }

    bool AnimationCurveValues::ApplyLayer(
        const AnimationCurveValues& layer,
        const float weight,
        const bool additive
    )
    {
        if (!std::isfinite(weight) || weight < 0 || weight > 1)
        {
            EE_CORE_ERROR("Animation curve layer weight must be in [0, 1].")
            return false;
        }

        auto result = *this;

        for (const auto& [name, value] : layer.m_Values)
        {
            const auto source = double(GetValue(name));
            const auto blended = additive
                ? source + double(value) * weight
                : source * (1.0 - weight) + double(value) * weight;

            if (!std::isfinite(blended) ||
                std::abs(blended) > std::numeric_limits<float>::max())
            {
                EE_CORE_ERROR("Animation curve layer composition exceeds the scalar range.")
                return false;
            }

            if (!result.SetValue(name, float(blended)))
                return false;
        }

        *this = std::move(result);
        return true;
    }
}
