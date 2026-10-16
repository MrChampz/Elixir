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

        /** @brief Create a sequence with ordered finite keys in [0, duration], or return null. */
        static Ref<AnimationSequence> Create(
            std::string name, Ref<const Skeleton> skeleton,
            float duration,
            std::vector<SBoneAnimationTrack> tracks
        );

        /**
         * @brief Sample a clamped time into a full local pose; leave output unchanged on failure.
         * @param referencePose Optional mesh-specific defaults for channels without keys.
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
