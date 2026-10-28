 #pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/Skeleton.h>
#include <Engine/Animation/RootMotion.h>
#include <Engine/Animation/AnimationCurve.h>

namespace Elixir
{
    /** @brief Selects interpolation between adjacent animation keys. */
    enum class EBoneInterpolation : uint8_t
    {
        Step, Linear
    };

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

    /** Defines a notification at a point or over an interval on an animation timeline. */
    struct ELIXIR_API SAnimationNotify
    {
        /** Application-defined event name, such as Footstep or AttackHit. */
        std::string Name;

        /** Finite time in seconds in [0, duration], including a nonlooping clip's end. */
        float Time = 0.0f;

        /**
         * Finite nonnegative duration in seconds, ending within the clip.
         * Zero emits Instant; positive durations emit Begin, Update, and End.
         */
        float Duration = 0.0f;

        /** Optional value of any copyable application-defined type, owned by this notify.
         * Sequence assets expose it read-only. Referenced resources keep their own ownership
         * rules. This runtime value does not provide automatic asset serialization.
         */
        std::any Payload;

        /**
         * @brief Check the name and finite time range within its owning sequence.
         * @param sequenceDuration Positive owning clip duration in seconds.
         */
        bool IsValid(float sequenceDuration) const;

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

    /** Describes a notification point or overlap on the playback reference timeline. */
    struct SAnimationNotifyInterval
    {
        /** Index into the sequence's immutable GetNotifies list. */
        size_t NotifyIndex = 0;

        /** Reference phase elapsed before the overlap starts. */
        double StartPhaseOffset = 0.0;

        /** Reference phase elapsed before the overlap ends. */
        double EndPhaseOffset = 0.0;

        /** The authored end was reached; true for instantaneous notifications. */
        bool Ends = false;
    };

    /** @brief Stores immutable animation tracks associated with a shared skeleton. */
    class ELIXIR_API Sequence final
    {
    public:
        /** Maximum authored events per update; queued cancellation Ends are additional. */
        static constexpr size_t MaxNotifyEventsPerUpdate = 4096;

        Sequence(const Sequence&) = delete;
        Sequence& operator=(const Sequence&) = delete;

        /**
         * @brief Create a sequence with finite keys and optional markers, or return null.
         * @param name Asset name.
         * @param skeleton Skeleton defining bone indices and default transforms.
         * @param duration Finite nonnegative cycle duration in seconds.
         * @param tracks Unique bone tracks with ordered keys in [0, duration].
         * @param syncMarkers Unique named markers at distinct times in [0, duration).
         * Markers are sorted during creation.
         * @param notifies Named events on a positive-duration timeline, sorted stably by time.
         * @param curves Unique named scalar tracks with finite ordered keys.
         */
        static Ref<Sequence> Create(
            std::string name,
            Ref<const Skeleton> skeleton,
            float duration,
            std::vector<SBoneAnimationTrack> tracks,
            std::vector<SAnimationSyncMarker> syncMarkers = {},
            std::vector<SAnimationNotify> notifies = {},
            std::vector<SAnimationCurve> curves = {}
        );

        /**
         * @brief Sample a clamped time into a full local pose; leave output unchanged on
         * failure.
         * @param time Finite sample time in seconds, clamped to the cycle duration.
         * @param output Receives one local transform per bone on success.
         * @param referencePose Optional mesh-specific defaults for channels without keys.
         */
        bool Sample(
            float time,
            SLocalPose& output,
            const SLocalPose& referencePose = {}
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
         * @brief Check whether this asset uses a compatible skeleton.
         * @param skeleton Skeleton whose bone layout will receive sampled poses.
         */
        bool IsCompatibleWith(const Ref<const Skeleton>& skeleton) const;

        /**
         * @brief Map unwrapped reference phase to continuous phase in this sequence.
         * @param phase Finite reference phase; complete cycles remain unwrapped.
         * @param reference Fixed marker timeline; incompatible markers use normalized phase.
         */
        double GetSynchronizedPhase(double phase, const Sequence& reference) const;

        /**
         * @brief Copy this immutable sequence with authored synchronization markers.
         * @param syncMarkers Valid cycle markers; empty removes existing markers.
         * @return A new sequence retaining tracks and metadata, or null for invalid markers.
         */
        Ref<Sequence> WithSyncMarkers(
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
        bool CanSynchronizeWith(const Sequence& reference) const;

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
        float GetSynchronizedTime(float phase, const Sequence& reference) const;

        /**
         * @brief Copy this sequence with authored events while retaining tracks and markers.
         * @param notifies Notifications within the clip; empty removes existing notifications.
         * @return A new sequence, or null for invalid names, times, or durations.
         */
        Ref<Sequence> WithNotifies(std::vector<SAnimationNotify> notifies) const;

        /** @brief Get immutable events sorted by time, retaining order for equal times. */
        const std::vector<SAnimationNotify>& GetNotifies() const { return m_Notifies; }

        /**
         * @brief Collect notification points and overlaps in chronological order.
         *
         * Instantaneous notifications exclude the interval start unless includeStart is true.
         * Notifications with duration include every positive overlap and exact start.
         * Loop-boundary instants at the clip end precede instants at time zero.
         * Keep output unchanged on invalid input or capacity overflow.
         *
         * @param startPhase Playback reference phase in [0, 1].
         * @param endPhase Unwrapped end phase at or after startPhase.
         * @param looping Include repeated cycles instead of clamping at phase one.
         * @param output Receives at most MaxNotifyEventsPerUpdate points and overlaps.
         * @param reference Optional marker reference used to map notification times.
         * @param includeStart Include instantaneous notifications at the interval start.
         */
        bool CollectNotifyIntervals(
            double startPhase,
            double endPhase,
            bool looping,
            std::vector<SAnimationNotifyInterval>& output,
            const Sequence* reference = nullptr,
            bool includeStart = false
        ) const;

        /**
         * @brief Sample scalar tracks at a clamped time; preserve output on failure.
         * @param time Finite sequence time in seconds.
         * @param output Receives all authored curves, including zero values.
         */
        bool SampleCurves(float time, AnimationCurveValues& output) const;

        /**
         * @brief Copy this sequence with scalar tracks, preserving all other metadata.
         * @param curves Unique named tracks; empty removes all curves.
         * @return A new sequence, or null for invalid names, keys, or interpolation.
         */
        Ref<Sequence> WithCurves(std::vector<SAnimationCurve> curves) const;

        /** @brief Get immutable authored scalar tracks. */
        const std::vector<SAnimationCurve>& GetCurves() const { return m_Curves; }

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
         * @param curves Validated scalar tracks.
         */
        Sequence(
            std::string name,
            Ref<const Skeleton> skeleton,
            float duration,
            std::vector<SBoneAnimationTrack> tracks,
            std::vector<SAnimationSyncMarker> syncMarkers,
            std::vector<SAnimationNotify> notifies,
            std::vector<SAnimationCurve> curves
        );

        std::string m_Name;
        Ref<const Skeleton> m_Skeleton;
        float m_Duration;
        std::vector<SBoneAnimationTrack> m_Tracks;
        std::vector<SAnimationSyncMarker> m_SyncMarkers;
        std::vector<SAnimationNotify> m_Notifies;
        std::vector<SAnimationCurve> m_Curves;
    };
}
