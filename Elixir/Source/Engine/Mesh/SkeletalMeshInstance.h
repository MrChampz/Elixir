#pragma once

#include <Engine/Mesh/SkeletalMesh.h>
#include <Engine/Animation/BlendSpace.h>
#include <initializer_list>

namespace Elixir
{
    /** @brief Owns a mesh instance's pose, playback, and animation transitions. */
    class ELIXIR_API SkeletalMeshInstance final
    {
    public:
        /**
         * @brief Bind a mesh for this instance's lifetime and initialize its reference pose.
         * @param mesh Non-null shared mesh asset retained for the entire instance lifetime.
         */
        explicit SkeletalMeshInstance(Ref<const SkeletalMesh> mesh);

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

        /** @brief Pause playback while preserving the current pose and time. */
        void Pause() { m_Playing = false; }

        /** @brief Resume the assigned sequence or blend space, if one exists. */
        void Resume();

        /** @brief Stop playback and restore this mesh's reference pose. */
        void Stop();

        /**
         * @brief Advance playback and transition; preserve state on failure.
         * @param dt Finite nonnegative elapsed time in seconds.
         */
        bool Update(float dt);

        /**
         * @brief Seek in the target sequence and cancel any transition on success.
         * @param time Finite time in seconds, wrapped or clamped by the playback mode.
         */
        bool Seek(float time);

        /** @brief Get the shared mesh asset. */
        const Ref<const SkeletalMesh>& GetMesh() const { return m_Mesh; }

        /**
         * @brief Set a complete local pose and stop playback; preserve state on failure.
         * @param pose One valid parent-relative transform per skeleton bone.
         */
        bool SetLocalPose(std::span<const SBoneTransform> pose);

        /** @brief Get one parent-relative transform per skeleton bone. */
        const std::vector<SBoneTransform>& GetLocalPose() const { return m_LocalPose; }

        /** @brief Get one skeleton-space matrix per skeleton bone. */
        const std::vector<glm::mat4>& GetModelPose() const { return m_ModelPose; }

        /** @brief Get target time in seconds using the current blend-space cycle duration. */
        float GetTime() const;

        /** @brief Get target cycle phase in [0, 1], or zero when no animation is assigned. */
        float GetNormalizedTime() const;

        /** @brief Check whether a transition still contributes to the current pose. */
        bool IsTransitioning() const { return m_Transition.Duration > 0.0f; }

        /** @brief Check whether Update advances playback or a transition. */
        bool IsPlaying() const { return m_Playing; }

      private:
        struct SPlayback
        {
            Ref<const AnimationSequence> Sequence;
            Ref<const BlendSpace> BlendSpace;
            std::vector<float> Values;

            /** Sequence seconds, or normalized cycle phase for a blend space. */
            float Time = 0.0f;
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
            float time
        ) const;

        /**
         * @brief Advance seconds or normalized phase using the current cycle duration.
         * @param playback Playback to advance.
         * @param dt Finite nonnegative elapsed time in seconds.
         */
        static float AdvancePlayback(const SPlayback& playback, float dt);

        /**
         * @brief Compose sampled local transforms after blending the fade source.
         * @param playback Target playback to evaluate and store on success.
         * @param source Source playback to sample when a fade is active.
         * @param weight Target weight in [0, 1].
         * @param time Target seconds or normalized phase.
         * @param sourceTime Source seconds or normalized phase.
         */
        bool Evaluate(
            const SPlayback& playback,
            const SPlayback& source,
            float weight,
            float time,
            float sourceTime
        );

        /**
         * @brief Get the cycle duration in seconds for assigned playback.
         * @param playback Sequence or blend-space playback.
         */
        static float GetDuration(const SPlayback& playback);

        STransition m_Transition;
        SPlayback m_Playback;
        bool m_Playing = false;

        const Ref<const SkeletalMesh> m_Mesh;
        std::vector<SBoneTransform> m_LocalPose;
        std::vector<glm::mat4> m_ModelPose;
    };
}
