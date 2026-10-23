#pragma once

#include <Engine/Animation/BlendSpace.h>
#include <Engine/Animation/AnimationNotifyEvent.h>
#include <Engine/Animation/AnimationNotifyStateEvent.h>
#include <Engine/Animation/BoneMask.h>
#include <Engine/Animation/AnimationMontage.h>

namespace Elixir
{
    /** Selects how a layer combines its sampled local pose with underlying layers. */
    enum class EAnimationLayerBlendMode : uint8_t
    {
        Override,
        Additive
    };

    /** @brief Owns masked pose composition settings and independent animation playback. */
    class ELIXIR_API AnimationLayer final
    {
        friend class SkeletalMeshInstance;

    public:
        /**
         * @brief Start a compatible sequence at time zero; preserve state on failure.
         * @param sequence Animation to play.
         * @param looping Wrap playback at the end of the animation.
         */
        bool Play(Ref<const AnimationSequence> sequence, bool looping = true);

        /**
         * @brief Blend into a compatible sequence starting at time zero.
         *
         * Both sequences advance during the fade. An interrupted fade starts from the
         * current blended pose. Zero duration switches immediately. Preserve state on failure.
         *
         * @param sequence Animation to play.
         * @param fadeDuration Finite nonnegative transition duration in seconds.
         * @param looping Wrap playback at the end of the target animation.
         */
        bool Play(
            Ref<const AnimationSequence> sequence,
            float fadeDuration,
            bool looping = true
        );

        /**
         * @brief Start any compatible blend space at cycle phase zero.
         * @param blendSpace Shared pose source, independent of its dimension count.
         * @param values One finite coordinate per axis; copied into playback storage.
         * @param fadeDuration Finite nonnegative fade duration in seconds.
         * @param looping Wrap the normalized cycle phase at one.
         */
        bool Play(
            Ref<const BlendSpace> blendSpace,
            std::span<const float> values,
            float fadeDuration = 0.0f,
            bool looping = true
        );

        /**
         * @brief Start a blend space with a temporary list of coordinates.
         * @param blendSpace Shared compatible pose source.
         * @param values One finite coordinate per axis.
         * @param fadeDuration Finite nonnegative fade duration in seconds.
         * @param looping Wrap the cycle phase at one.
         */
        bool Play(
            Ref<const BlendSpace> blendSpace,
            const std::initializer_list<float> values,
            const float fadeDuration = 0.0f,
            const bool looping = true
        )
        {
            return Play(
                std::move(blendSpace),
                std::span(values.begin(), values.size()),
                fadeDuration,
                looping
            );
        }

        /** @brief Pause playback while preserving the current pose and time. */
        void Pause()
        {
            m_Playing = false;
            m_Montage.Playing = false;
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
        bool SetAdditiveReferencePose(std::span<const SBoneTransform> pose);

        /** @brief Get the custom additive reference, or the mesh's local reference pose. */
        const std::vector<SBoneTransform>& GetAdditiveReferencePose() const
        {
            return m_AdditiveReferencePose.empty()
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
         */
        std::span<const SAnimationNotifyEvent> GetNotifyEvents() const
        {
            return m_NotifyEvents;
        }

        /**
         * @brief Get Begin, Update, and End events from the last successful instance update.
         *
         * Playback changes queue cancelled Ends for the next successful update, including
         * a paused or zero-delta update. Pause retains active windows without Updates.
         * Consumers must process each successful batch once. Removed layers transfer their
         * queued Ends to the owning instance. Events retain their sequence and typed payload.
         */
        std::span<const SAnimationNotifyStateEvent> GetNotifyStateEvents() const
        {
            return m_NotifyStateEvents;
        }

        /**
         * @brief Set coordinates for the next update without resetting phase or fade.
         * @param values One finite coordinate per active blend-space axis; copied on success.
         */
        bool SetBlendSpaceValue(std::span<const float> values);

        /**
         * @brief Set blend coordinates using a temporary list.
         * @param values One finite coordinate per active blend-space axis.
         */
        bool SetBlendSpaceValue(const std::initializer_list<float> values)
        {
            return SetBlendSpaceValue(std::span(values.begin(), values.size()));
        }

        /**
         * @brief Set a complete local pose and stop playback; preserve state on failure.
         * @param pose One valid parent-relative transform per skeleton bone.
         */
        bool SetLocalPose(std::span<const SBoneTransform> pose);

        /** @brief Get one parent-relative transform per skeleton bone. */
        const std::vector<SBoneTransform>& GetLocalPose() const
        {
            return m_Montage.Montage ? m_MontagePose : m_LocalPose;
        }

        /** @brief Get source time in seconds, independent of any active montage. */
        float GetTime() const;

        /** @brief Get source phase in [0, 1], independent of any active montage. */
        float GetNormalizedTime() const;

        /** @brief Check whether playback or a manual pose has been assigned since Stop. */
        bool HasPose() const { return m_Assigned || bool(m_Montage.Montage); }

        /** @brief Check whether the owning instance advances this layer's playback. */
        bool IsPlaying() const
        {
            return m_Playing || (m_Montage.Montage && m_Montage.Playing);
        }

        /** @brief Check whether the underlying source has an active playback transition. */
        bool IsTransitioning() const { return m_Transition.Duration > 0.0f; }

        /** @brief Get the active action, or null after its exit completes. */
        const Ref<const AnimationMontage>& GetMontage() const { return m_Montage.Montage; }

        /** @brief Get the sequence position in seconds; section jumps can move it backward. */
        double GetMontageTime() const { return m_Montage.Cursor.Time; }

        /** @brief Get the active action's current contribution in [0, 1]. */
        float GetMontageWeight() const { return GetMontageWeight(m_Montage.Cursor); }

      private:
        struct SMontageCursor
        {
            double Time = 0.0;
            double Elapsed = 0.0;
            size_t Section = 0;
            bool IncludeNotifyStart = true;
            bool Finished = false;
        };

        struct SMontageInterval
        {
            SMontageCursor Start;
            double Duration = 0.0;
            double TimeOffset = 0.0;
        };

        struct SMontageEvaluation
        {
            SMontageCursor Cursor;
            std::vector<SMontageInterval> Intervals;
            double ActiveDuration = 0.0;
        };

        struct SActiveNotifyState
        {
            Ref<const AnimationSequence> Sequence;
            size_t Index = 0;
            uint64_t ActivationId = 0;
        };

        struct SNotifyStateEvaluation
        {
            std::vector<SActiveNotifyState> Active;
            std::vector<SAnimationNotifyStateEvent> Events;
            uint64_t NextActivationId = 1;
        };

        struct SLayerEvaluation
        {
            std::vector<SBoneTransform> LocalPose;
            std::vector<SBoneTransform> PlaybackPose;
            SMontageEvaluation Montage;
            std::vector<SAnimationNotifyEvent> NotifyEvents;
            SNotifyStateEvaluation NotifyStates;
            double PlaybackTime = 0.0;
            double TransitionSourceTime = 0.0;
            float TransitionElapsed = 0.0f;
            bool Advance = false;
            bool ConsumeInitialNotifies = false;
        };

        struct SPlayback
        {
            Ref<const AnimationSequence> Sequence;
            Ref<const BlendSpace> BlendSpace;
            std::vector<float> Values;

            /** Sequence seconds, or normalized cycle phase for a blend space. */
            double Time = 0.0;

            bool IncludeNotifyStart = true;
            bool Looping = true;
        };

        struct STransition
        {
            SPlayback Playback;

            /** Visible source pose when playback is absent or a fade was interrupted. */
            std::vector<SBoneTransform> Pose;
            float Duration = 0.0f;
            float Elapsed = 0.0f;
        };

        struct SMontagePlayback
        {
            Ref<const AnimationMontage> Montage;
            SMontageCursor Cursor;
            std::vector<std::optional<size_t>> NextSections;
            double StopTime = -1.0;
            float StopDuration = 0.0f;
            float StopWeight = 0.0f;
            bool Playing = false;

            /** Frozen visible pose used only when replacing another montage. */
            std::vector<SBoneTransform> SourcePose;
        };

        /**
         * @brief Sample the continuing source before applying the action.
         * @param dt Finite nonnegative elapsed seconds.
         * @param output Receives source pose, clocks, and events.
         */
        bool EvaluatePlayback(float dt, SLayerEvaluation& output) const;

        /**
         * @brief Sample the action and blend it over an evaluated source pose.
         * @param cursor Sequence position, section, and elapsed playback seconds.
         * @param pose Source pose modified on success.
         */
        bool BlendMontage(const SMontageCursor& cursor, std::vector<SBoneTransform>& pose) const;

        /**
         * @brief Get the action envelope at one time.
         * @param cursor Sequence position and elapsed playback seconds.
         */
        float GetMontageWeight(const SMontageCursor& cursor) const;

        /**
         * @brief Get the elapsed time when the current route releases its layer.
         * @param cursor Current section and position; cyclic routes have no natural end.
         */
        double GetMontageEnd(const SMontageCursor& cursor) const;

        /**
         * @brief Traverse section links without changing playback state.
         * @param dt Finite nonnegative elapsed seconds.
         * @param output Receives the final cursor and visited forward intervals.
         * @return False when more than 4096 section intervals are needed.
         */
        bool EvaluateMontage(float dt, SMontageEvaluation& output) const;

        /**
         * @brief Gather action events and any source events after the action ends.
         * @param dt Elapsed frame seconds.
         * @param evaluation Previously validated section traversal.
         * @param output Receives events in chronological frame order.
         */
        bool CollectMontageNotifyEvents(
            float dt,
            const SMontageEvaluation& evaluation,
            std::vector<SAnimationNotifyEvent>& output
        ) const;

        /**
         * @brief Average the linear action envelope over a forward interval.
         * @param interval Forward interval within one section.
         */
        float GetAverageMontageWeight(const SMontageInterval& interval) const;

        /**
         * @brief Extract continuing source movement without including the action.
         * @param dt Elapsed seconds to extract.
         * @param boneIndex Root-motion bone index.
         * @param output Receives source movement on success.
         * @param offset Seconds from the current source clock to the interval start.
         */
        bool ExtractSourceRootMotion(
            float dt,
            uint32_t boneIndex,
            SRootMotionDelta& output,
            float offset = 0
        ) const;

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
            std::span<const SBoneTransform> referencePose
        );

        /**
         * @brief Copy playback for a transactional mesh-instance change.
         * @param other Layer with the same name, mask, skeleton, and reference pose.
         */
        AnimationLayer(const AnimationLayer& other) = default;

        /**
         * @brief Commit playback prepared for the same mesh instance.
         * @param other Layer with the same name, mask, skeleton, and reference pose.
         */
        AnimationLayer& operator=(const AnimationLayer& other) = default;

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
            std::span<SBoneTransform> output,
            std::span<const SBoneTransform> pose
        ) const;

        /**
         * @brief Evaluate this layer without changing its playback state.
         * @param dt Finite nonnegative elapsed time in seconds.
         * @param output Receives the local pose, clocks, and events on success.
         */
        bool Evaluate(float dt, SLayerEvaluation& output) const;

        /**
         * @brief Apply an evaluation accepted by the owning mesh instance.
         * @param evaluation Evaluated local pose, clocks, and events to consume.
         */
        void Apply(SLayerEvaluation evaluation);

        /**
         * @brief Sample and blend a target pose without changing playback state.
         * @param playback Target sequence or blend space.
         * @param source Source playback for an active fade.
         * @param weight Target contribution in [0, 1].
         * @param time Target sample time in seconds or phase.
         * @param sourceTime Source sample time in seconds or phase.
         * @param output Receives a valid local pose on success.
         */
        bool SamplePose(
            const SPlayback& playback,
            const SPlayback& source,
            float weight,
            double time,
            double sourceTime,
            std::vector<SBoneTransform>& output
        ) const;

        /**
         * @brief Start validated playback and capture the fade source.
         * @param playback Compatible target sequence or blend space at time zero.
         * @param fadeDuration Finite nonnegative duration in seconds.
         */
        bool StartPlayback(SPlayback playback, float fadeDuration);

        /**
         * @brief Sample playback into a full local pose.
         * @param playback Assigned sequence or blend space and its sample time.
         * @param output Local transforms to receive the sample.
         * @param time Seconds for a sequence or normalized phase for a blend space.
         */
        bool SamplePlayback(
            const SPlayback& playback,
            std::vector<SBoneTransform>& output,
            double time
        ) const;

        /**
         * @brief Advance seconds or normalized phase using the current cycle duration.
         * @param playback Playback to advance.
         * @param dt Finite nonnegative elapsed time in seconds.
         */
        static double AdvancePlayback(const SPlayback& playback, float dt);

        /**
         * @brief Compose sampled local transforms after blending the fade source.
         * @param playback Target playback to evaluate and store on success.
         * @param source Source playback to sample when a fade is active.
         * @param weight Target weight in [0, 1].
         * @param time Target seconds or normalized phase.
         * @param sourceTime Source seconds or normalized phase.
         */
        bool SetPlayback(
            const SPlayback& playback,
            const SPlayback& source,
            float weight,
            double time,
            double sourceTime
        );

        /**
         * @brief Get the cycle duration in seconds for assigned playback.
         * @param playback Sequence or blend-space playback.
         */
        static float GetDuration(const SPlayback& playback);

        /**
         * @brief Extract base playback movement and blend active fade contributions.
         * @param dt Finite nonnegative elapsed time in seconds.
         * @param boneIndex Selected root bone in the mesh skeleton.
         * @param output Receives movement; snapshots and paused playback contribute identity.
         * @param evaluation Validated traversal reused for section movement.
         */
        bool ExtractRootMotion(
            float dt,
            uint32_t boneIndex,
            SRootMotionDelta& output,
            const SLayerEvaluation& evaluation
        ) const;

        /**
         * @brief Extract movement from one playback interval without advancing its clock.
         * @param playback Sequence or blend space, including its current time and looping.
         * @param dt Finite nonnegative elapsed time in seconds.
         * @param boneIndex Selected root bone in the mesh skeleton.
         * @param output Receives parent-local movement on success.
         * @param offset Seconds from the playback clock to the interval start.
         */
        bool ExtractPlaybackRootMotion(
            const SPlayback& playback,
            float dt,
            uint32_t boneIndex,
            SRootMotionDelta& output,
            float offset = 0
        ) const;

        /**
         * @brief Gather events from one source without changing its clock.
         * @param playback Source, clock, and initial-event policy.
         * @param dt Elapsed seconds to collect.
         * @param output Receives source events with this layer's name.
         */
        bool CollectPlaybackNotifyEvents(
            const SPlayback& playback,
            float dt,
            std::vector<SAnimationNotifyEvent>& output
        ) const;

        /** @brief Queue cancelled Ends and release the layer's currently active windows. */
        void CancelNotifyStates();

        /**
         * @brief Evaluate windows on the pose and section playback timeline.
         * @param dt Elapsed frame seconds.
         * @param evaluation Validated pose and montage traversal receiving window state.
         */
        bool EvaluateNotifyStates(float dt, SLayerEvaluation& evaluation) const;

        /**
         * @brief Advance active windows on one sequence or blend-space reference timeline.
         * @param playback Source, phase, coordinates, and looping mode.
         * @param dt Active interval seconds.
         * @param offset Frame seconds preceding this interval.
         * @param output Transactional active windows, events, and activation counter.
         */
        bool EvaluatePlaybackNotifyStates(
            const SPlayback& playback, double dt, double offset,
            SNotifyStateEvaluation& output
        ) const;

        /**
         * @brief End all active windows at a timeline discontinuity or playback completion.
         * @param output Transactional active windows and events.
         * @param offset Frame seconds when windows end.
         * @param cancelled True for interruption; false for timeline completion.
         */
        void EndNotifyStates(
            SNotifyStateEvaluation& output, double offset, bool cancelled
        ) const;

        std::string m_Name;
        Ref<const BoneMask> m_Mask;
        float m_Weight = 1.0f;
        EAnimationLayerBlendMode m_BlendMode = EAnimationLayerBlendMode::Override;

        /** Empty storage selects the mesh reference without duplicating its transforms. */
        std::vector<SBoneTransform> m_AdditiveReferencePose;

        /** The fixed first layer always uses Override, a full-body mask, and weight one. */
        bool m_Base = false;

        SMontagePlayback m_Montage;
        std::vector<SBoneTransform> m_MontagePose;

        STransition m_Transition;
        SPlayback m_Playback;
        bool m_Playing = false;

        /** Distinguishes an explicit pose from an unassigned layer's reference pose. */
        bool m_Assigned = false;

        std::vector<SAnimationNotifyEvent> m_NotifyEvents;
        std::vector<SActiveNotifyState> m_ActiveNotifyStates;
        std::vector<SAnimationNotifyStateEvent> m_NotifyStateEvents;
        std::vector<SAnimationNotifyStateEvent> m_PendingNotifyStateEnds;
        uint64_t m_NextNotifyStateActivationId = 1;

        Ref<const Skeleton> m_Skeleton;
        std::vector<SBoneTransform> m_ReferencePose;
        std::vector<SBoneTransform> m_LocalPose;
    };
}
