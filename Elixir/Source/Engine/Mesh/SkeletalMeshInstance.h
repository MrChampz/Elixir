#pragma once

#include <Engine/Animation/AnimationSequence.h>
#include <Engine/Mesh/SkeletalMesh.h>

namespace Elixir
{
    /** @brief Owns a mesh instance's pose and single-sequence playback state. */
    class ELIXIR_API SkeletalMeshInstance final
    {
    public:
        /** @brief Create an empty instance, or initialize a mesh in its reference pose. */
        explicit SkeletalMeshInstance(Ref<const SkeletalMesh> mesh = nullptr);

        /** @brief Start a compatible sequence at time zero; preserve state on failure. */
        bool Play(Ref<const AnimationSequence> sequence, bool looping = true);

        /** @brief Pause playback while preserving the current pose and time. */
        void Pause() { m_Playing = false; }

        /** @brief Resume the assigned sequence, if one exists. */
        void Resume() { m_Playing = m_Sequence && m_Sequence->GetDuration() > 0.0f; }

        /** @brief Stop playback and restore this mesh's reference pose. */
        void Stop();

        /** @brief Advance by finite nonnegative seconds; preserve the last valid pose on failure. */
        bool Update(float deltaSeconds);

        /** @brief Seek to a finite time in seconds, wrapping or clamping according to playback mode. */
        bool Seek(float time);

        /** @brief Assign a mesh and reset playback and pose; null clears the instance. */
        void SetMesh(Ref<const SkeletalMesh> mesh);

        /** @brief Get the shared mesh asset. */
        const Ref<const SkeletalMesh>& GetMesh() const { return m_Mesh; }

        /** @brief Set a complete local pose and stop sequence playback; preserve state on failure. */
        bool SetLocalPose(std::span<const SBoneTransform> pose);

        /** @brief Get one parent-relative transform per skeleton bone. */
        const std::vector<SBoneTransform>& GetLocalPose() const { return m_LocalPose; }

        /** @brief Get one skeleton-space matrix per skeleton bone. */
        const std::vector<glm::mat4>& GetModelPose() const { return m_ModelPose; }

        /** @brief Get the current sequence time in seconds. */
        float GetTime() const { return m_Time; }

        /** @brief Check whether Update advances the sequence. */
        bool IsPlaying() const { return m_Playing; }

    private:
        // Sample a sequence without changing state on failure.
        bool Evaluate(const Ref<const AnimationSequence>& sequence, float time);

        Ref<const SkeletalMesh> m_Mesh;
        Ref<const AnimationSequence> m_Sequence;
        std::vector<SBoneTransform> m_LocalPose;
        std::vector<glm::mat4> m_ModelPose;
        float m_Time = 0.0f;
        bool m_Looping = true;
        bool m_Playing = false;
    };
}
