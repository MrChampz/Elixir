#pragma once

#include <Engine/Core/Animation/Animator.h>
#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    /**
     * @brief Plays keyframe tracks that update one widget.
     *
     * The animation belongs to its owner, not to Widget. Each track supplies its own binding,
     * so callers can animate any public widget or style property.
     */
    class ELIXIR_API WidgetAnimation
    {
      public:
        /**
         * @brief Create an animation for one widget.
         * @param target Widget updated by each track.
         */
        explicit WidgetAnimation(Ref<Widget> target);

        /**
         * @brief Add a keyframe track and its widget binding.
         * @tparam TValue Value sampled from the curve.
         * @param curve Keyframes to play.
         * @param apply Stores each sampled value in the target widget.
         */
        template<typename TValue, typename TApply>
        void AddTrack(
            AnimationCurve<TValue> curve,
            TApply apply
        )
        {
            m_Tracks.emplace_back(
                [curve = std::move(curve), apply = std::move(apply)](
                    Animator& animator,
                    const Ref<Widget>& target,
                    std::function<void()> onComplete
                )
                {
                    animator.Bind<TValue>(
                        curve,
                        [target, apply](const TValue& value) { apply(*target, value); },
                        std::move(onComplete)
                    );
                }
            );
        }

        /** @brief Remove every track from this animation. */
        void ClearTracks();

        /** @brief Start the tracks from their first keyframes. */
        void Play();

        /** @brief Stop the tracks at their current values. */
        void Stop();

        /** @brief Advance the active tracks by one frame. */
        void Update(Timestep frameTime);

        /** @brief Return true while this animation has active tracks. */
        bool IsPlaying() const { return m_IsPlaying; }

        /**
         * @brief Set the callback that runs after every track finishes.
         * @param callback Callback to run once per completed playback.
         */
        void OnFinished(std::function<void()> callback)
        {
            m_OnFinished = std::move(callback);
        }

      private:
        using TTrack = std::function<void(Animator&, const Ref<Widget>&, std::function<void()>)>;

        Ref<Widget> m_Target;
        Animator m_Animator;
        std::vector<TTrack> m_Tracks;
        size_t m_PendingTracks = 0;
        bool m_IsPlaying = false;
        std::function<void()> m_OnFinished;
    };
}
