#pragma once

#include <Engine/Animation/AnimationSequence.h>
#include <Engine/Mesh/SkeletalMesh.h>

namespace Elixir
{
    /** @brief Owns a mesh instance's pose, playback, and animation transitions. */
    class ELIXIR_API SkeletalMeshInstance final
    {
    public:
        /**
         * @brief Create an empty instance, or initialize a mesh in its reference pose.
         * @param mesh Shared mesh asset, or null for an empty instance.
         */
        explicit SkeletalMeshInstance(Ref<const SkeletalMesh> mesh = nullptr);

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

        /** @brief Pause playback while preserving the current pose and time. */
        void Pause() { m_Playing = false; }

        /** @brief Resume the assigned sequence, if one exists. */
        void Resume()
        {
            m_Playing = m_Sequence && (m_Sequence->GetDuration() > 0.0f || IsTransitioning());
        }

        /** @brief Stop playback and restore this mesh's reference pose. */
        void Stop();

        /**
         * @brief Advance playback and transition; preserve state on failure.
         * @param deltaSeconds Finite nonnegative elapsed time in seconds.
         */
        bool Update(float deltaSeconds);

        /**
         * @brief Seek in the target sequence and cancel any transition on success.
         * @param time Finite time in seconds, wrapped or clamped by the playback mode.
         */
        bool Seek(float time);

        /**
         * @brief Assign a mesh and reset playback and pose; null clears the instance.
         * @param mesh Shared mesh asset, or null to clear the instance.
         */
        void SetMesh(Ref<const SkeletalMesh> mesh);

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

        /** @brief Get the target sequence time in seconds. */
        float GetTime() const { return m_Time; }

        /** @brief Check whether a transition still contributes to the current pose. */
        bool IsTransitioning() const { return m_Transition.Duration > 0.0f; }

        /** @brief Check whether Update advances playback or a transition. */
        bool IsPlaying() const { return m_Playing; }

    private:
        /**
         * @brief Sample and blend local transforms before composing the model pose.
         * @param sequence Target animation.
         * @param time Target sample time in seconds.
         * @param sourceTime Source sample time in seconds.
         * @param weight Target weight in [0, 1]; one bypasses the transition.
         */
        bool Evaluate(
            const Ref<const AnimationSequence>& sequence,
            float time,
            float sourceTime = 0.0f,
            float weight = 1.0f
        );

        /** @brief Retains the source of a fade into the current sequence. */
        struct STransition
        {
            Ref<const AnimationSequence> Sequence;

            /** Source pose when no sequence exists or a previous fade was interrupted. */
            std::vector<SBoneTransform> Pose;

            /** Source sequence sample time in seconds. */
            float Time = 0.0f;

            /** Total fade duration in seconds; zero means no active transition. */
            float Duration = 0.0f;

            /** Elapsed fade time in seconds, independent of either sequence duration. */
            float Elapsed = 0.0f;
            bool Looping = true;
        };

        STransition m_Transition;

        Ref<const SkeletalMesh> m_Mesh;
        Ref<const AnimationSequence> m_Sequence;
        std::vector<SBoneTransform> m_LocalPose;
        std::vector<glm::mat4> m_ModelPose;
        float m_Time = 0.0f;
        bool m_Looping = true;
        bool m_Playing = false;
    };
}
