 #pragma once

#include <Engine/Animation/RootMotion.h>

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

    /** Describes an instantaneous event authored on an animation timeline. */
    struct SAnimationNotify
    {
        /** Application-defined event name, such as Footstep or AttackHit. */
        std::string Name;

        /** Finite time in seconds in [0, duration], including a nonlooping clip's end. */
        float Time = 0.0f;

        /** Optional value of any copyable application-defined type, owned by this notify.
         * Sequence assets expose it read-only. Referenced resources keep their own ownership
         * rules. This runtime value does not provide automatic asset serialization.
         */
        std::any Payload;

        /**
         * @brief Read the payload without conversion or exceptions.
         * @tparam T Exact stored value type, such as an enum or an application-defined struct.
         * @return A borrowed read-only value, or null if empty or holding another type.
         */
        template<typename T>
        const T* GetPayload() const
        {
            return std::any_cast<T>(&Payload);
        }
    };

    /** Identifies one event crossed while advancing a cycle. */
    struct SAnimationNotifyOccurrence
    {
        /** Index into the source sequence's sorted notify list. */
        size_t NotifyIndex = 0;

        /** Elapsed reference phase from the start of this update to the event. */
        double PhaseOffset = 0.0;
    };

    /** @brief Stores immutable animation tracks associated with a shared skeleton. */
    class ELIXIR_API AnimationSequence final
    {
    public:
        /** Maximum events emitted by one successful update; larger batches fail safely. */
        static constexpr size_t MaxNotifyEventsPerUpdate = 4096;

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
         * @param notifies Named events on a positive-duration timeline, sorted stably by time.
         */
        static Ref<AnimationSequence> Create(
            std::string name, Ref<const Skeleton> skeleton,
            float duration,
            std::vector<SBoneAnimationTrack> tracks,
            std::vector<SAnimationSyncMarker> syncMarkers = {},
            std::vector<SAnimationNotify> notifies = {}
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
         * @brief Extract rigid movement of a selected bone over a forward phase interval.
         *
         * Looping intervals may cross cycle boundaries in either positive or negative
         * phase space. At most 4096 cycles may be crossed per call. Nonlooping phases clamp
         * to [0, 1]. Output is unchanged on failure. Scale remains in the sampled pose.
         *
         * @param startPhase Finite start phase; one unit represents one complete cycle.
         * @param endPhase Finite unwrapped phase greater than or equal to startPhase.
         * @param looping Accumulate complete cycles instead of clamping to the clip ends.
         * @param reference Mesh-specific local reference transform of the selected bone.
         * @param output Receives parent-local movement, conjugated through the reference root.
         * @param boneIndex Bone to extract; its ancestors must remain in the reference pose.
         */
        bool ExtractRootMotion(
            double startPhase,
            double endPhase,
            bool looping,
            const SBoneTransform& reference,
            SRootMotionDelta& output,
            uint32_t boneIndex = 0
        ) const;

        /**
         * @brief Map unwrapped reference phase to continuous phase in this sequence.
         * @param phase Finite reference phase; complete cycles remain unwrapped.
         * @param reference Fixed marker timeline; incompatible markers use normalized phase.
         */
        double GetSynchronizedPhase(double phase, const AnimationSequence& reference) const;

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

        /**
         * @brief Copy this sequence with authored events while retaining tracks and markers.
         * @param notifies Events in [0, duration]; empty removes existing events.
         * @return A new sequence, or null for invalid event names or times.
         */
        Ref<AnimationSequence> WithNotifies(std::vector<SAnimationNotify> notifies) const;

        /** @brief Get immutable events sorted by time, retaining order for equal times. */
        const std::vector<SAnimationNotify>& GetNotifies() const { return m_Notifies; }

        /**
         * @brief Collect events crossed during a forward update, in chronological order.
         *
         * The interval excludes its start and includes its end. Looping intervals may cross
         * several cycles. End events precede start events at the same loop boundary.
         * Leave output unchanged for invalid input or more than MaxNotifyEventsPerUpdate.
         *
         * @param startPhase Wrapped phase in [0, 1] before the update.
         * @param endPhase Unwrapped phase at or after startPhase; nonlooping ends clamp to one.
         * @param looping Include events from every crossed cycle.
         * @param output Receives the events for this interval, replacing earlier contents.
         * @param reference Optional marker reference; null uses this clip's normalized phase.
         * @param includeStart Include start events on the first positive playback update.
         */
        bool CollectNotifyOccurrences(
            double startPhase,
            double endPhase,
            bool looping,
            std::vector<SAnimationNotifyOccurrence>& output,
            const AnimationSequence* reference = nullptr,
            bool includeStart = false
        ) const;

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
         * @param notifies Validated events in stable ascending time order.
         */
        AnimationSequence(
            std::string name,
            Ref<const Skeleton> skeleton,
            float duration,
            std::vector<SBoneAnimationTrack> tracks,
            std::vector<SAnimationSyncMarker> syncMarkers,
            std::vector<SAnimationNotify> notifies
        );

        std::string m_Name;
        Ref<const Skeleton> m_Skeleton;
        float m_Duration;
        std::vector<SBoneAnimationTrack> m_Tracks;
        std::vector<SAnimationSyncMarker> m_SyncMarkers;
        std::vector<SAnimationNotify> m_Notifies;
    };
}
