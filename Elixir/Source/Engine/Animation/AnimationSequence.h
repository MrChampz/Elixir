#pragma once

#include <Engine/Animation/Skeleton.h>

namespace Elixir
{
    /** @brief Selects interpolation between adjacent animation keys. */
    enum class EBoneInterpolation : uint8_t { Step, Linear };

    /** @brief Stores a vector animation value at a time in seconds. */
    struct SVectorAnimationKey
    {
        float Time = 0.0f;
        glm::vec3 Value{};
    };

    /** @brief Stores a unit quaternion at a time in seconds. */
    struct SRotationAnimationKey
    {
        float Time = 0.0f;
        glm::quat Value{ 1.0f, 0.0f, 0.0f, 0.0f };
    };

    /** @brief Animates one bone; empty channels retain the reference transform. */
    struct SBoneAnimationTrack
    {
        uint32_t BoneIndex = 0;
        std::vector<SVectorAnimationKey> Translations;
        std::vector<SRotationAnimationKey> Rotations;
        std::vector<SVectorAnimationKey> Scales;
        EBoneInterpolation TranslationInterpolation = EBoneInterpolation::Linear;
        EBoneInterpolation RotationInterpolation = EBoneInterpolation::Linear;
        EBoneInterpolation ScaleInterpolation = EBoneInterpolation::Linear;
    };

    /** @brief Stores immutable animation tracks associated with a shared skeleton. */
    class ELIXIR_API AnimationSequence final
    {
    public:
        AnimationSequence(const AnimationSequence&) = delete;
        AnimationSequence& operator=(const AnimationSequence&) = delete;

        /**
         * @brief Create an animation sequence from validated bone tracks.
         * @param name Asset display name.
         * @param skeleton Skeleton that defines track bone indices.
         * @param duration Finite nonnegative duration in seconds.
         * @param tracks Tracks with unique valid bone indices and ordered finite keys in
         * [0, duration].
         * @return The sequence, or null for invalid input.
         */
        static Ref<AnimationSequence> Create(
            std::string name, Ref<const Skeleton> skeleton,
            float duration,
            std::vector<SBoneAnimationTrack> tracks
        );

        /**
         * @brief Sample a clamped time into a complete local pose.
         * @param time Finite sample time in seconds, clamped to [0, duration].
         * @param output Receives one local transform per bone on success.
         * @param referencePose Optional mesh defaults; empty uses the skeleton reference pose.
         * @return False for invalid input or output transforms; output remains unchanged on
         * failure.
         */
        bool Sample(
            float time,
            std::vector<SBoneTransform>& output,
            std::span<const SBoneTransform> referencePose = {}
        ) const;

        /** @brief Get the asset name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get the skeleton that defines track bone indices. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Skeleton; }

        /** @brief Get the duration in seconds. */
        float GetDuration() const { return m_Duration; }

    private:
        /**
         * @brief Store validated animation tracks.
         * @param name Asset display name.
         * @param skeleton Skeleton that defines track bone indices.
         * @param duration Finite nonnegative duration in seconds.
         * @param tracks Tracks with unique valid bone indices and ordered finite keys in
         * [0, duration].
         */
        AnimationSequence(
            std::string name,
            Ref<const Skeleton> skeleton,
            float duration,
            std::vector<SBoneAnimationTrack> tracks
        );

        std::string m_Name;
        Ref<const Skeleton> m_Skeleton;
        float m_Duration;
        std::vector<SBoneAnimationTrack> m_Tracks;
    };
}
