#pragma once

#include <Engine/Animation/BlendSpace.h>
#include <Engine/Animation/AnimationNotifyEvent.h>
#include <Engine/Animation/BoneMask.h>

namespace Elixir
{
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
        void Pause() { m_Playing = false; }

        /** @brief Resume the assigned sequence or blend space, if one exists. */
        void Resume();

        /** @brief Stop playback and restore the reference pose. */
        void Stop();

        /**
         * @brief Seek silently and clear pending events and the active transition on success.
         * @param time Finite time in seconds, wrapped or clamped by the playback mode.
         */
        bool Seek(float time);

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

        /** @brief Get the fixed skeleton, or null for invalid construction. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Skeleton; }

        /**
         * @brief Get this layer's events from the last successful instance update.
         *
         * The owning instance advances this layer. Read events on that same thread.
         * The span remains valid until the next successful instance update, Play, Seek,
         * Stop, or SetLocalPose. Failed operations keep
         * the previous batch; consumers must process events only after successful updates.
         */
        std::span<const SAnimationNotifyEvent> GetNotifyEvents() const
        {
            return m_NotifyEvents;
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
        const std::vector<SBoneTransform>& GetLocalPose() const { return m_LocalPose; }

        /** @brief Get target time in seconds using the current blend-space cycle duration. */
        float GetTime() const;

        /** @brief Get target cycle phase in [0, 1], or zero when no animation is assigned. */
        float GetNormalizedTime() const;

        /** @brief Check whether playback or a manual pose has been assigned since Stop. */
        bool HasPose() const { return m_Assigned; }

        /** @brief Check whether the owning instance advances this layer's playback. */
        bool IsPlaying() const { return m_Playing; }

        /** @brief Check whether a transition still contributes to the current pose. */
        bool IsTransitioning() const { return m_Transition.Duration > 0.0f; }

      private:
        struct SLayerEvaluation
        {
            std::vector<SBoneTransform> LocalPose;
            std::vector<SAnimationNotifyEvent> NotifyEvents;
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
         * @brief Gather target events before committing a playback update.
         * @param dt Positive elapsed time in seconds; zero emits no events.
         * @param output Receives occurrences retaining the selected source asset.
         */
        bool CollectNotifyEvents(float dt, std::vector<SAnimationNotifyEvent>& output) const;

        std::string m_Name;
        Ref<const BoneMask> m_Mask;
        float m_Weight = 1.0f;

        /** The fixed first layer always uses a full-body mask and weight one. */
        bool m_Base = false;

        STransition m_Transition;
        SPlayback m_Playback;
        bool m_Playing = false;

        /** Distinguishes an explicit pose from an unassigned layer's reference pose. */
        bool m_Assigned = false;

        std::vector<SAnimationNotifyEvent> m_NotifyEvents;

        Ref<const Skeleton> m_Skeleton;
        std::vector<SBoneTransform> m_ReferencePose;
        std::vector<SBoneTransform> m_LocalPose;
    };
}
