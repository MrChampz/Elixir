#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/BoneMask.h>
#include <Engine/Animation/AnimationPlayback.h>
#include <Engine/Animation/AnimationMontagePlayback.h>
#include <Engine/Animation/AnimationNotifyEvent.h>
#include <Engine/Animation/AnimationNotifyTracker.h>
#include <Engine/Animation/AnimationTransition.h>

namespace Elixir
{
    /** How a layer contributes to the existing base layer pose. */
    enum class EAnimationLayerBlendMode : uint8_t
    {
        Override, Additive
    };

    /** Name reserved for the full-body base control. */
    inline constexpr std::string BaseLayerName = "Base";

    /** Candidate pose, events, and clock positions for one layer. */
    struct SAnimationLayerEvaluation
    {
        SLocalPose LocalPose;

        /** Continuing source pose before applying a montage. */
        SLocalPose PlaybackPose;

        SAnimationMontageEvaluation Montage;

        SNotifyEvaluation Notifies;

        /** Normalized candidate positions for the target and outgoing playbacks. */
        double PlaybackPhase = 0;

        double TransitionSourcePhase = 0;

        /** Candidate fade time in seconds. */
        float TransitionElapsed = 0;

        bool Advance = false;

        /** Consume start-point events only after a positive time advance is published. */
        bool ConsumeInitialNotifies = false;
    };

    /** @brief Owns masked pose composition settings and independent animation playback. */
    class ELIXIR_API AnimationLayer final
    {
        friend class Animation;
        friend class BlendNodeEvaluator;
        friend class AnimationNodeQuery;
        friend class AnimationPlayback;

    public:
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
         * @brief Play a one-shot action over this layer without replacing its source.
         *
         * The source and its state machine continue advancing underneath the action.
         * The layer mask and blend mode apply to the resulting pose. A replacement fades
         * from the visible pose; a non-interruptible action rejects replacement.
         * Underlying events are suppressed until the action ends. Montage events retain
         * their sequence and layer name, including events during entrance and exit fades.
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
         * @brief Fade back to this layer's continuing source.
         * @param blendOut Exit seconds; zero returns immediately, empty uses asset settings.
         * @return False for invalid duration or a non-interruptible action.
         */
        bool StopMontage(std::optional<float> blendOut = std::nullopt);

        /** @brief Get this instance-local layer's name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get the immutable mask, or null for an invalid mesh instance. */
        const Ref<const BoneMask>& GetMask() const { return m_Mask; }

        /** @brief Get the layer's contribution in [0, 1]. */
        float GetWeight() const { return m_Weight; }

        /**
         * @brief Set the layer's contribution without resetting its playback.
         * @param weight Finite weight in [0, 1]; the base layer requires one.
         * @return False for invalid input; zero mutes events but still advances playback.
         */
        bool SetWeight(float weight);

        /** @brief Get the layer's pose composition mode. */
        EAnimationLayerBlendMode GetBlendMode() const { return m_BlendMode; }

        /**
         * @brief Set how this layer combines its pose on the next instance update.
         * @param mode Override blends absolute poses; Additive applies reference differences.
         * @return False for an unknown mode or an additive base layer; playback is preserved.
         */
        bool SetBlendMode(EAnimationLayerBlendMode mode);

        /**
         * @brief Set the local reference pose used to calculate additive differences.
         *
         * The same reference applies to every sequence and blend-space sample on this layer.
         * Channels without animation keys retain the mesh's reference transforms.
         * Changes affect composition on the next instance update without seeking playback.
         *
         * @param pose One valid local transform per bone; empty restores the mesh reference.
         * @return False for invalid input; the previous reference remains unchanged.
         */
        bool SetAdditiveReferencePose(const SLocalPose& pose);

        /** @brief Get the custom additive reference, or the mesh's local reference pose. */
        const SLocalPose& GetAdditiveReferencePose() const
        {
            return m_AdditiveReferencePose.LocalTransforms.empty()
                ? m_ReferencePose
                : m_AdditiveReferencePose;
        }

        /** @brief Get the fixed skeleton, or null for invalid construction. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Skeleton; }

        /**
         * @brief Get this layer's events from the last successful instance update.
         *
         * The owning instance advances this layer. Read events on that same thread.
         * The span remains valid until the next successful instance update, Play, Seek,
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

        /** @brief Check whether the owning instance advances this layer's playback. */
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
         * @brief Extract layer movement from playback, transitions, and active montage
         * intervals.
         * @param interval Root reference transform, bone index, duration, and offset to
         * extract.
         * @param output Receives movement on success; paused playback and snapshots contribute
         * identity.
         * @param evaluation Validated candidate layer frame containing visited montage
         * intervals.
         * @return True on success; output remains unchanged on failure.
         */
        bool ExtractRootMotion(
            const SRootMotionIntervalContext& interval,
            SRootMotionDelta& output,
            const SAnimationLayerEvaluation& evaluation
        ) const;

        /**
         * @brief Extract layer movement without publishing new playback clocks.
         * @param interval Root reference, bone index, and requested frame subinterval.
         * @param output Receives movement on success; unchanged on failure.
         */
        bool ExtractRootMotion(
            const SRootMotionIntervalContext& interval,
            SRootMotionDelta& output
        ) const;

      private:
        /**
         * @brief Sample the continuing source before applying the action.
         * @param dt Finite nonnegative elapsed seconds.
         * @param output Receives source pose, clocks, and events.
         */
        bool EvaluatePlayback(float dt, SAnimationLayerEvaluation& output) const;

        /**
         * @brief Create a layer bound to a mask and mesh-specific reference pose.
         * @param name Unique instance-local name.
         * @param mask Immutable mask defining the skeleton and bone weights.
         * @param weight Validated layer contribution in [0, 1].
         * @param referencePose One local reference transform per bone.
         */
        AnimationLayer(
            std::string name,
            Ref<const BoneMask> mask,
            float weight,
            const SLocalPose& referencePose
        );

        /**
         * @brief Copy playback for a transactional mesh-instance change.
         * @param other Layer with the same name, mask, skeleton, and reference pose.
         */
        AnimationLayer(const AnimationLayer& other) = default;

        /**
         * @brief Replace a candidate belonging to the same animation instance.
         * @param other Layer with the same fixed composition context.
         */
        AnimationLayer& operator=(const AnimationLayer& other) = default;

        /**
         * @brief Publish a prepared layer without cloning its runtime sources.
         * @param other Layer with the same fixed composition context.
         */
        AnimationLayer& operator=(AnimationLayer&& other) noexcept = default;

        /**
         * @brief Blend this layer into a candidate local pose using its mode and mask.
         *
         * Additive translation uses parent-relative differences. Weighted rotation
         * differences pre-multiply the current local rotation. Scale uses per-axis ratios.
         * A failure may change the candidate, which must then be discarded by the caller.
         *
         * @param output Underlying local pose to modify.
         * @param pose This layer's sampled absolute local pose.
         * @return False for invalid dimensions or a nonfinite or singular result.
         */
        bool BlendPose(
            SLocalPose& output,
            const SLocalPose& pose
        ) const;

        /**
         * @brief Evaluate this layer without changing its playback state.
         * @param dt Finite nonnegative elapsed time in seconds.
         * @param output Receives the local pose, clocks, and events on success.
         * @param notifiesEnabled Whether enclosing composition can publish source events.
         * @param paused Freeze source decisions and clocks while delivering queued Ends.
         */
        bool Evaluate(
            float dt,
            SAnimationLayerEvaluation& output,
            bool paused = false,
            bool notifiesEnabled = true
        ) const;

        /**
         * @brief Apply an evaluation accepted by the owning mesh instance.
         * @param evaluation Evaluated local pose, clocks, and events to consume.
         */
        void Apply(SAnimationLayerEvaluation evaluation);

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
            SAnimationLayerEvaluation& evaluation,
            bool resetActivations = false
        ) const;

        std::string m_Name;
        EAnimationLayerBlendMode m_BlendMode = EAnimationLayerBlendMode::Override;
        float m_Weight = 1;

        bool m_Base = false;
        bool m_Assigned = false;
        bool m_Playing = false;

        Ref<AnimationParameters> m_Parameters = CreateRef<AnimationParameters>();
        std::vector<size_t> m_InputBindings;

        AnimationMontagePlayback m_Montage;
        SLocalPose m_MontagePose;

        AnimationTransition m_Transition;
        AnimationPlayback m_Playback;

        std::vector<SAnimationNotifyEvent> m_NotifyEvents;
        AnimationNotifyTracker m_Notifies;

        Ref<const BoneMask> m_Mask;
        Ref<const Skeleton> m_Skeleton;
        SLocalPose m_ReferencePose;
        SLocalPose m_AdditiveReferencePose;
        SLocalPose m_LocalPose;
    };
}
