#pragma once

#include <Engine/Core/Core.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

namespace Elixir
{
    /** @brief Selects how a curve moves to its next keyframe. */
    enum class EKeyframeInterpolation : uint8_t
    {
        Constant,
        Linear,
        EaseIn,
        EaseOut,
        EaseInOut,
    };

    /** @brief Stores one value and its position on a curve. */
    template<typename TValue>
    struct SKeyframe
    {
        float Time = 0.0f;
        TValue Value{};
        EKeyframeInterpolation Interpolation = EKeyframeInterpolation::Linear;
    };

    /**
     * @brief Stores keyframes and samples values between them.
     *
     * The curve is independent from a playback target. It can describe values for GUI
     * properties, particle parameters, or any other system that supplies an interpolator.
     *
     * @tparam TValue Value stored by each keyframe.
     */
    template<typename TValue>
    class AnimationCurve
    {
      public:
        using TInterpolator = std::function<TValue(const TValue&, const TValue&, float)>;

        /** @brief Create a curve that uses linear interpolation where supported. */
        AnimationCurve() : m_Interpolator(InterpolateLinearly) {}

        /**
         * @brief Create a curve with a custom value interpolator.
         * @param interpolator Function that blends two values using progress in [0, 1].
         */
        explicit AnimationCurve(TInterpolator interpolator)
          : m_Interpolator(std::move(interpolator)) {}

        /**
         * @brief Add or replace a keyframe at its time.
         * @param keyframe Value and time to store.
         */
        void AddKey(SKeyframe<TValue> keyframe)
        {
            const auto it = std::lower_bound(
                m_Keys.begin(),
                m_Keys.end(),
                keyframe.Time,
                [](const SKeyframe<TValue>& item, const float time) { return item.Time < time; }
            );
            if (it != m_Keys.end() && it->Time == keyframe.Time)
            {
                *it = std::move(keyframe);
                return;
            }

            m_Keys.insert(it, std::move(keyframe));
        }

        /** @brief Remove every keyframe from this curve. */
        void Clear() { m_Keys.clear(); }

        /** @brief Return true when this curve has no values to sample. */
        bool IsEmpty() const { return m_Keys.empty(); }

        /** @brief Return the time of the final keyframe, or zero when empty. */
        float GetDuration() const { return m_Keys.empty() ? 0.0f : m_Keys.back().Time; }

        /**
         * @brief Sample a value at a time in seconds.
         * @param time Time to sample. Values before or after the curve use the nearest key.
         * @return The sampled value.
         */
        TValue Sample(const float time) const
        {
            EE_CORE_ASSERT(!m_Keys.empty(), "AnimationCurve::Sample requires at least one keyframe");
            if (m_Keys.empty()) return {};
            if (time <= m_Keys.front().Time) return m_Keys.front().Value;
            if (time >= m_Keys.back().Time) return m_Keys.back().Value;

            const auto next = std::upper_bound(
                m_Keys.begin(),
                m_Keys.end(),
                time,
                [](const float value, const SKeyframe<TValue>& item) { return value < item.Time; }
            );
            const auto previous = std::prev(next);
            if (previous->Interpolation == EKeyframeInterpolation::Constant)
                return previous->Value;

            const float progress = (time - previous->Time) / (next->Time - previous->Time);
            return m_Interpolator(previous->Value, next->Value, ApplyInterpolation(previous->Interpolation, progress));
        }

      private:
        static float ApplyInterpolation(const EKeyframeInterpolation interpolation, const float progress)
        {
            switch (interpolation)
            {
            case EKeyframeInterpolation::EaseIn: return progress * progress;
            case EKeyframeInterpolation::EaseOut: return 1.0f - (1.0f - progress) * (1.0f - progress);
            case EKeyframeInterpolation::EaseInOut:
                return progress < 0.5f
                    ? 2.0f * progress * progress
                    : 1.0f - std::pow(-2.0f * progress + 2.0f, 2.0f) * 0.5f;
            default: return progress;
            }
        }

        static TValue InterpolateLinearly(const TValue& from, const TValue& to, const float progress)
        {
            if constexpr (requires { from + (to - from) * progress; })
            {
                return from + (to - from) * progress;
            }
            else
            {
                EE_CORE_ASSERT(false, "AnimationCurve requires a custom interpolator for this value type");
                return from;
            }
        }

        std::vector<SKeyframe<TValue>> m_Keys;
        TInterpolator m_Interpolator;
    };
}
