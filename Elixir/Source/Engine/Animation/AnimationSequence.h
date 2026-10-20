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

    /** Names a matching point within one repeating animation cycle. */
    struct SAnimationSyncMarker
    {
        /** Unique contact or event name within this cycle, such as LeftFootPlant. */
        std::string Name;

        /** Finite time in seconds in [0, duration); the cycle end aliases time zero. */
        float Time = 0.0f;
    };

    /** @brief Stores immutable animation tracks associated with a shared skeleton. */
    class ELIXIR_API AnimationSequence final
    {
    public:
        AnimationSequence(const AnimationSequence&) = delete;
        AnimationSequence& operator=(const AnimationSequence&) = delete;

        /**
         * @brief Create a sequence with finite keys and optional markers, or return null.
         * @param name Asset name.
         * @param skeleton Skeleton defining bone indices and default transforms.
         * @param duration Finite nonnegative cycle duration in seconds.
         * @param tracks Unique bone tracks with ordered keys in [0, duration].
         * @param syncMarkers Unique named markers at distinct times in [0, duration).
         * Markers are sorted during creation.
         */
        static Ref<AnimationSequence> Create(
            std::string name, Ref<const Skeleton> skeleton,
            float duration,
            std::vector<SBoneAnimationTrack> tracks,
            std::vector<SAnimationSyncMarker> syncMarkers = {}
        );

        /**
         * @brief Sample a clamped time into a full local pose; leave output unchanged on failure.
         * @param time Finite sample time in seconds, clamped to the cycle duration.
         * @param output Receives one local transform per bone on success.
         * @param referencePose Optional mesh-specific defaults for channels without keys.
         */
        bool Sample(
            float time,
            std::vector<SBoneTransform>& output,
            std::span<const SBoneTransform> referencePose = {}
        ) const;

        /**
         * @brief Copy this immutable sequence with authored synchronization markers.
         * @param syncMarkers Valid cycle markers; empty removes existing markers.
         * @return A new sequence retaining tracks and metadata, or null for invalid markers.
         */
        Ref<AnimationSequence> WithSyncMarkers(
            std::vector<SAnimationSyncMarker> syncMarkers
        ) const;

        /** @brief Get synchronization markers sorted by time within the cycle. */
        const std::vector<SAnimationSyncMarker>& GetSyncMarkers() const
        {
            return m_SyncMarkers;
        }

        /**
         * @brief Check whether two cycles have the same marker names in cyclic order.
         * @param reference Sequence whose marker intervals define the shared phase.
         * @return False if either sequence has no markers or their layouts differ.
         */
        bool CanSynchronizeWith(const AnimationSequence& reference) const;

        /**
         * @brief Map a reference cycle phase into this sequence's matching marker interval.
         *
         * Interpolate between matching markers, including the interval across the cycle end.
         * Use ordinary normalized time if either track is missing or layouts differ.
         * Markers describe repeating cycles. An offset may map phase one inside a clip.
         *
         * @param phase Finite reference phase, clamped to [0, 1].
         * @param reference Sequence defining the shared phase timeline.
         * @return Sample time in seconds, or zero for a nonfinite phase.
         */
        float GetSynchronizedTime(float phase, const AnimationSequence& reference) const;

        /** @brief Get the asset name. */
        const std::string& GetName() const { return m_Name; }

        /** @brief Get the skeleton that defines track bone indices. */
        const Ref<const Skeleton>& GetSkeleton() const { return m_Skeleton; }

        /** @brief Get the duration in seconds. */
        float GetDuration() const { return m_Duration; }

    private:
        /**
         * @brief Store validated tracks and sorted markers.
         * @param name Asset name.
         * @param skeleton Skeleton shared by tracks.
         * @param duration Cycle duration in seconds.
         * @param tracks Validated bone tracks.
         * @param syncMarkers Validated cycle markers in ascending time order.
         */
        AnimationSequence(
            std::string name,
            Ref<const Skeleton> skeleton,
            float duration,
            std::vector<SBoneAnimationTrack> tracks,
            std::vector<SAnimationSyncMarker> syncMarkers
        );

        std::string m_Name;
        Ref<const Skeleton> m_Skeleton;
        float m_Duration;
        std::vector<SBoneAnimationTrack> m_Tracks;
        std::vector<SAnimationSyncMarker> m_SyncMarkers;
    };
}
