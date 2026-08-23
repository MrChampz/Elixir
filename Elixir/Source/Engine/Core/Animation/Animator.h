#pragma once

#include <Engine/Core/Animation/AnimationCurve.h>
#include <Engine/Core/Timer.h>

#include <functional>
#include <vector>

namespace Elixir
{
    /** @brief Evaluates one or more keyframe curves over time. */
    class ELIXIR_API Animator
    {
      public:
        using AnimationId = uint64_t;

        /**
         * @brief Bind a curve to a value receiver.
         * @tparam TValue Value sampled from the curve.
         * @param curve Keyframes to evaluate.
         * @param apply Receives each sampled value.
         * @param onComplete Called after the final keyframe is applied.
         * @return Identifier used to stop this binding.
         */
        template<typename TValue>
        AnimationId Bind(
            const AnimationCurve<TValue>& curve,
            std::function<void(const TValue&)> apply,
            std::function<void()> onComplete = {}
        )
        {
            EE_CORE_ASSERT(!curve.IsEmpty(), "Animator::Bind requires a curve with keyframes");
            if (curve.IsEmpty()) return 0;

            return Bind(
                curve.GetDuration(),
                [curve, apply = std::move(apply)](const float time) { apply(curve.Sample(time)); },
                std::move(onComplete)
            );
        }

        /** @brief Stop one binding without calling its completion callback. */
        void Stop(AnimationId id);

        /** @brief Stop every binding without calling completion callbacks. */
        void StopAll();

        /** @brief Advance every active binding by one frame. */
        void Update(Timestep frameTime);

        /** @brief Return true while at least one binding is active. */
        bool IsAnimating() const { return !m_Tracks.empty(); }

      private:
        AnimationId Bind(
            float duration,
            std::function<void(float)> apply,
            std::function<void()> onComplete
        );

        struct STrack
        {
            AnimationId Id;
            float Duration = 0.0f;
            float Elapsed = 0.0f;
            std::function<void(float)> Apply;
            std::function<void()> OnComplete;
        };

        std::vector<STrack> m_Tracks;
        AnimationId m_NextId = 1;
    };
}
