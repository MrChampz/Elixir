#pragma once

#include <Engine/Animation/AnimationPlayback.h>
#include <Engine/Animation/AnimationMontagePlayback.h>
#include <Engine/Animation/AnimationNotifyTracker.h>
#include <Engine/Animation/AnimationTransition.h>

namespace Elixir
{
    /** Candidate pose, notifications, and clocks produced by one player. */
    struct SAnimationPlayerEvaluation
    {
        /** Visible local pose after applying any active montage. */
        SLocalPose LocalPose;

        /** Continuing source pose before applying a montage. */
        SLocalPose PlaybackPose;

        /** Candidate montage cursor and visited section intervals. */
        SAnimationMontageEvaluation Montage;

        /** Candidate notifications and active windows, published with the pose. */
        SNotifyEvaluation Notifies;

        /** Normalized candidate positions for the target and outgoing playbacks. */
        double PlaybackPhase = 0;

        /** Candidate normalized position of the outgoing transition source. */
        double TransitionSourcePhase = 0;

        /** Candidate fade time in seconds. */
        float TransitionElapsed = 0;

        /** Publish source clocks when this evaluation is applied. */
        bool Advance = false;

        /** Consume start-point events only after a positive time advance is published. */
        bool ConsumeInitialNotifies = false;
    };

    /** Owns source playback, transitions, montages, and notification evaluation. */
    class ELIXIR_API AnimationPlayer final
    {
        friend class AnimationLayer;

    public:
        /**
         * @brief Create playback bound to a skeleton and reference pose.
         * @param name Nonempty stable name used to identify this player's notifications.
         * @param skeleton Skeleton required by every assigned source.
         * @param referencePose One local reference transform per skeleton bone.
         */
        AnimationPlayer(
            std::string name,
            Ref<const Skeleton> skeleton,
            const SLocalPose& referencePose
        );

        /**
         * @brief Copy playback into an independent player with the same sampling context.
         * @param other Player whose sources, clocks, and notification state are copied.
         */
        AnimationPlayer(const AnimationPlayer& other) = default;

        /**
         * @brief Prepare this player without publishing new clocks or events.
         * @param dt Finite nonnegative elapsed time in seconds.
         * @param output Receives the local pose, clocks, and events on success.
         * @param paused Freeze source decisions and clocks while delivering queued Ends.
         * @param notifiesEnabled Whether enclosing composition can publish source events.
         */
        bool Evaluate(
            float dt,
            SAnimationPlayerEvaluation& output,
            bool paused = false,
            bool notifiesEnabled = true
        );

        /**
         * @brief Publish a successful evaluation from this player.
         * @param evaluation Evaluated local pose, clocks, and events to consume.
         */
        void Apply(SAnimationPlayerEvaluation evaluation);

        /**
         * @brief Start a compatible animation source at its initial position; preserve state
         * on failure.
         * @param source Animation source to play.
         * @param looping Wrap playback at the end of the animation.
         */
        bool Play(Ref<const AnimationNode> source, bool looping = true);

        /**
         * @brief Blend into a compatible animation source at its initial position.
         *
         * Both sequences advance during the fade. An interrupted fade starts from the
         * current blended pose. Zero duration switches immediately. Preserve state on failure.
         *
         * @param source Animation source to play.
         * @param fadeDuration Finite nonnegative transition duration in seconds.
         * @param looping Wrap playback at the end of the target animation.
         */
        bool Play(
            Ref<const AnimationNode> source,
            float fadeDuration,
            bool looping = true
        );

        /**
         * @brief Start a compatible animation source with explicit numeric inputs.
         * @param source Animation source to play.
         * @param values Numeric inputs required by the concrete source.
         * @param fadeDuration Finite nonnegative fade duration in seconds.
         * @param looping Wrap the normalized cycle phase at one.
         */
        bool Play(
            Ref<const AnimationNode> source,
            std::span<const float> values,
            float fadeDuration = 0.0f,
            bool looping = true
        );

        /**
         * @brief Start an animation source with a temporary list of numeric inputs.
         * @param source Animation source to play.
         * @param values Numeric inputs required by the concrete source.
         * @param fadeDuration Finite nonnegative fade duration in seconds.
         * @param looping Wrap the cycle phase at one.
         */
        bool Play(
            Ref<const AnimationNode> source,
            const std::initializer_list<float> values,
            const float fadeDuration = 0.0f,
            const bool looping = true
        )
        {
            return Play(
                std::move(source),
                std::span(values.begin(), values.size()),
                fadeDuration,
                looping
            );
        }

        /** @brief Pause playback while preserving the current pose and time. */
        void Pause()
        {
            m_Playing = false;
            m_Montage.Pause();
        }

        /** @brief Resume the assigned sequence or blend space, if one exists. */
        void Resume();

        /** @brief Stop playback and restore the reference pose. */
        void Stop();

        /**
         * @brief Seek without instantaneous events and clear the active source transition.
         *
         * Active source windows queue cancelled Ends. An active montage keeps its windows.
         *
         * @param time Finite time in seconds, wrapped or clamped by the playback mode.
         */
        bool Seek(float time);

        /**
         * @brief Play a one-shot action over the continuing source.
         *
         * The source and its state machine continue advancing underneath the action.
         * A replacement fades from the visible pose; a non-interruptible action rejects
         * replacement. Composition settings are applied separately to the resulting pose.
         * Underlying events are suppressed until the action ends. Montage events retain
         * their sequence and player name, including events during entrance and exit fades.
         *
         * @param montage Compatible action asset, retained until its exit completes.
         * @param startSection Optional starting section name; empty selects the first section.
         * @return False for invalid input or an active non-interruptible action.
         */
        bool PlayMontage(
            Ref<const AnimationMontage> montage,
            std::string_view startSection = {}
        );

        /**
         * @brief Override a section's continuation for this playback instance.
         * @param from Existing section whose outgoing link changes.
         * @param to Existing destination; empty ends playback after the source section.
         * @return False for unknown names or a link change that interrupts the exit fade.
         */
        bool SetNextMontageSection(std::string_view from, std::string_view to);

        /**
         * @brief Jump silently to a section without extracting the skipped movement.
         *
         * Preserve the entrance clock and pause state. The next positive update includes
         * destination start events. Section jumps switch pose immediately without a fade.
         *
         * @param name Existing destination section.
         * @return False for invalid input or while an explicit StopMontage exit is active.
         */
        bool JumpToMontageSection(std::string_view name);

        /** @brief Get the current section name, or empty when no montage is active. */
        std::string_view GetCurrentMontageSection() const;

        /**
         * @brief Fade back to the continuing source.
         * @param blendOut Exit seconds; zero returns immediately, empty uses asset settings.
         * @return False for invalid duration or a non-interruptible action.
         */
        bool StopMontage(std::optional<float> blendOut = std::nullopt);

        /** @brief Check whether construction supplied a valid skeleton and reference pose. */
        bool IsValid() const { return bool(m_Skeleton); }

        /** @brief Get the stable name attached to this player's notifications. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Inspect the active source playback. */
        const AnimationPlayback& GetPlayback() const { return m_Playback; }

        /** @brief Get the skeleton required by assigned sources. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Skeleton; }

        /** @brief Get the reference pose used to sample missing animation channels. */
        const SLocalPose& GetReferencePose() const { return m_ReferencePose; }

        /** @brief Inspect cancellation Ends queued for the next accepted evaluation. */
        std::span<const SAnimationNotifyEvent> GetPendingNotifyEnds() const
        {
            return m_Notifies.GetPendingEnds();
        }

        /** @brief Cancel active notifications without changing playback clocks. */
        void CancelNotifies();

        /**
         * @brief Get this player's events from the last accepted evaluation.
         *
         * Read events on the same thread that evaluates and applies this player.
         * The span remains valid until the next accepted evaluation, Play, Seek,
         * Stop, SetLocalPose, PlayMontage, or an immediate StopMontage. Failed operations keep
         * the previous batch; consumers must process events only after successful updates.
         * Instant, Begin, Update, and End share this list, ordered by TimeOffset.
         * Playback changes queue cancelled Ends for the next successful update, including
         * paused or zero-delta updates. Pause retains active windows without Updates.
         * Queued cancellation Ends do not consume the authored event budget.
         */
        std::span<const SAnimationNotifyEvent> GetNotifyEvents() const
        {
            return m_NotifyEvents;
        }

        /**
         * @brief Set source inputs without resetting its position or transition.
         * @param values Finite numeric inputs validated by the concrete source.
         */
        bool SetSourceInputs(std::span<const float> values);

        /**
         * @brief Set source inputs using a temporary list.
         * @param values One finite coordinate per active blend-space axis.
         */
        bool SetSourceInputs(const std::initializer_list<float> values)
        {
            return SetSourceInputs(std::span(values.begin(), values.size()));
        }

        /**
         * @brief Set a complete local pose and stop playback; preserve state on failure.
         * @param pose One valid parent-relative transform per skeleton bone.
         */
        bool SetLocalPose(const SLocalPose& pose);

        /** @brief Inspect scalar channels sampled with the published local pose. */
        const AnimationCurveValues& GetCurveValues() const { return GetLocalPose().Curves; }

        /** @brief Get one parent-relative transform per skeleton bone. */
        const SLocalPose& GetLocalPose() const
        {
            return m_Montage.GetAsset() ? m_MontagePose : m_LocalPose;
        }

        /** @brief Get source time in seconds, independent of any active montage. */
        float GetTime() const { return m_Playback.GetTime(); }

        /** @brief Get source phase in [0, 1], independent of any active montage. */
        float GetNormalizedTime() const;

        /** @brief Check whether playback or a manual pose has been assigned since Stop. */
        bool HasPose() const { return m_Assigned || bool(m_Montage.GetAsset()); }

        /** @brief Check whether source or montage playback is running. */
        bool IsPlaying() const
        {
            return m_Playing || (m_Montage.GetAsset() && m_Montage.IsPlaying());
        }

        /** @brief Check whether the underlying source has an active playback transition. */
        bool IsTransitioning() const
        {
            return m_Transition.IsActive() || m_Playback.IsTransitioning();
        }

        /** @brief Get the active action, or null after its exit completes. */
        const Ref<const AnimationMontage>& GetMontage() const { return m_Montage.GetAsset(); }

        /** @brief Get the sequence position in seconds; section jumps can move it backward. */
        double GetMontageTime() const { return m_Montage.GetCursor().Time; }

        /** @brief Get the active action's current contribution in [0, 1]. */
        float GetMontageWeight() const { return m_Montage.GetWeight(m_Montage.GetCursor()); }

        /**
         * @brief Bind character values and resolve this source's numeric input names.
         * @param parameters Retained character parameter values.
         * @param bindings Numeric parameter names in source input order.
         */
        bool BindParameters(
            Ref<AnimationParameters> parameters,
            std::span<const std::string> bindings = {}
        );

        /** @brief Get the committed state of the assigned source, if it is a machine. */
        std::string_view GetCurrentState() const { return m_Playback.GetCurrentState(); }

        /**
         * @brief Extract player movement from playback, transitions, and active montage
         * intervals.
         * @param interval Root reference transform, bone index, duration, and offset to
         * extract.
         * @param output Receives movement on success; paused playback and snapshots contribute
         * identity.
         * @param evaluation Validated candidate player frame containing visited montage
         * intervals.
         * @return True on success; output remains unchanged on failure.
         */
        bool ExtractRootMotion(
            const SRootMotionIntervalContext& interval,
            SRootMotionDelta& output,
            const SAnimationPlayerEvaluation& evaluation
        ) const;

        /**
         * @brief Extract player movement without publishing new playback clocks.
         * @param interval Root reference, bone index, and requested frame subinterval.
         * @param output Receives movement on success; unchanged on failure.
         */
        bool ExtractRootMotion(
            const SRootMotionIntervalContext& interval,
            SRootMotionDelta& output
        ) const;

    private:
        /**
         * @brief Replace a player as part of an accepted layer candidate.
         * @param other Player with the same fixed layer sampling context.
         */
        AnimationPlayer& operator=(const AnimationPlayer& other) = default;

        /**
         * @brief Publish a candidate player without cloning its animation sources.
         * @param other Player with the same fixed layer sampling context.
         */
        AnimationPlayer& operator=(AnimationPlayer&& other) noexcept = default;

        /**
         * @brief Enable notifications or queue cancellation Ends when disabling them.
         * @param enabled Allow this player to emit notifications during evaluation.
         */
        void SetNotifiesEnabled(bool enabled);

        /**
         * @brief Sample the continuing source before applying the action.
         * @param dt Finite nonnegative elapsed seconds.
         * @param output Receives source pose, clocks, and events.
         */
        bool EvaluatePlayback(float dt, SAnimationPlayerEvaluation& output) const;

        /**
         * @brief Start validated playback and capture the fade source.
         * @param playback Compatible target sequence or blend space at time zero.
         * @param fadeDuration Finite nonnegative duration in seconds.
         */
        bool StartPlayback(AnimationPlayback playback, float fadeDuration);

        /**
         * @brief Compose sampled local transforms after blending the fade source.
         * @param playback Target playback to evaluate and store on success.
         * @param source Source playback to sample when a fade is active.
         * @param weight Target weight in [0, 1].
         * @param time Target normalized phase.
         * @param sourceTime Source normalized phase.
         */
        bool SetPlayback(
            const AnimationPlayback& playback,
            const AnimationPlayback& source,
            float weight,
            double time,
            double sourceTime
        );

        /**
         * @brief Evaluate notifications on the pose and section playback timeline.
         * @param resetActivations Start fresh candidate windows after enclosing suppression.
         * @param dt Elapsed frame seconds.
         * @param evaluation Validated pose and montage intervals receiving notification
         * results.
         */
        bool EvaluateNotifyEvents(
            float dt,
            SAnimationPlayerEvaluation& evaluation,
            bool resetActivations = false
        ) const;

        std::string m_Name;

        Ref<const Skeleton> m_Skeleton;
        SLocalPose m_ReferencePose;
        SLocalPose m_LocalPose;

        bool m_Assigned = false;
        bool m_Playing = false;

        /** Composition event policy; disabling events still allows playback to advance. */
        bool m_NotifiesEnabled = true;

        Ref<AnimationParameters> m_Parameters = CreateRef<AnimationParameters>();
        std::vector<size_t> m_InputBindings;

        AnimationMontagePlayback m_Montage;
        SLocalPose m_MontagePose;

        AnimationTransition m_Transition;
        AnimationPlayback m_Playback;

        std::vector<SAnimationNotifyEvent> m_NotifyEvents;
        AnimationNotifyTracker m_Notifies;
    };
}
