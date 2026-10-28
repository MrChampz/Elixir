#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/AnimationMontage.h>
#include <Engine/Animation/AnimationPlaybackInterval.h>

namespace Elixir
{
    /** Tracks sequence position and active playback time along a montage's section route. */
    struct SAnimationMontageCursor
    {
        /** Position in the source sequence, in seconds; section jumps may move it backward. */
        double Time = 0;

        /** Active seconds since playback started; pauses and skipped sections add no time. */
        double Elapsed = 0;

        /** Index of the current section in the montage's GetSections() list. */
        size_t Section = 0;

        /** Include notifications exactly at Time when processing the next visited interval. */
        bool IncludeNotifyStart = true;

        /** The route ended or the requested stop fade completed. */
        bool Finished = false;
    };

    /** Describes one continuous section interval actually visited during an evaluation. */
    struct SAnimationMontageInterval
    {
        /** Cursor snapshot before traversing this interval. */
        SAnimationMontageCursor Start;

        /** Traversed interval length in seconds, excluding skipped sequence ranges. */
        double Duration = 0;

        /** Seconds from the current evaluation's start to this interval's start. */
        double TimeOffset = 0;
    };

    /** Holds a candidate cursor and visited intervals without publishing playback changes. */
    struct SAnimationMontageEvaluation
    {
        /** Proposed final cursor, published only when the complete layer frame succeeds. */
        SAnimationMontageCursor Cursor;

        /** Visited intervals in playback order; skipped sections produce no intervals. */
        std::vector<SAnimationMontageInterval> Intervals;

        /**
         * Total traversed seconds in this evaluation. May be shorter than the frame when
         * playback finishes, leaving the remaining time to the continuing animation.
         */
        double ActiveDuration = 0;
    };

    /** Executes montage routes, envelopes, and interruptions independently of a layer. */
    class ELIXIR_API AnimationMontagePlayback
    {
    public:
        /**
         * @brief Start an action and validate its initial visible pose before replacement.
         * @param montage Retained immutable action.
         * @param section Optional initial section name.
         * @param visible Current visible pose for an interrupted entrance.
         * @param visibleCurves Scalar channels captured with the visible pose.
         * @param continuing Continuing source pose under the action.
         * @param continuingCurves Scalar channels from the continuing source.
         * @param referencePose Mesh-specific reference pose.
         * @param output Receives the action's initial visible pose.
         * @param outputCurves Receives the initial visible scalar channels.
         */
        bool Play(
            Ref<const AnimationMontage> montage,
            std::string_view section,
            const SLocalPose& visible,
            const AnimationCurveValues& visibleCurves,
            const SLocalPose& continuing,
            const AnimationCurveValues& continuingCurves,
            const SLocalPose& referencePose,
            SLocalPose& output,
            AnimationCurveValues& outputCurves
        );

        /**
         * @brief Change a section's continuation if the visible exit stays continuous.
         * @param from Existing source section.
         * @param to Destination name, or empty to finish playback.
         */
        bool SetNextSection(std::string_view from, std::string_view to);

        /**
         * @brief Jump silently and validate the resulting pose.
         * @param name Existing destination section.
         * @param continuing Continuing source pose.
         * @param continuingCurves Scalar channels from the continuing source.
         * @param referencePose Mesh-specific defaults.
         * @param output Receives the pose at the destination.
         * @param outputCurves Receives scalar channels at the destination.
         */
        bool Jump(
            std::string_view name,
            const SLocalPose& continuing,
            const AnimationCurveValues& continuingCurves,
            const SLocalPose& referencePose,
            SLocalPose& output,
            AnimationCurveValues& outputCurves
        );

        /**
         * @brief Stop immediately or fade out without restarting an existing exit.
         * @param blendOut Optional nonnegative exit duration.
         */
        bool Stop(std::optional<float> blendOut);

        /** @brief Pause without changing the cursor. */
        void Pause() { m_State.Playing = false; }

        /** @brief Resume an assigned montage. */
        void Resume() { m_State.Playing = bool(m_State.Montage); }

        /** @brief Check whether an action is assigned. */
        bool IsActive() const { return bool(m_State.Montage); }

        /** @brief Check whether the action advances. */
        bool IsPlaying() const { return m_State.Playing; }

        /** @brief Check whether an explicit exit is active. */
        bool IsStopping() const { return m_State.StopTime >= 0; }

        /** @brief Get the immutable action, if assigned. */
        const Ref<const AnimationMontage>& GetAsset() const { return m_State.Montage; }

        /** @brief Get the committed cursor. */
        const SAnimationMontageCursor& GetCursor() const { return m_State.Cursor; }

        /** @brief Get the current section name, or empty when inactive. */
        std::string_view GetCurrentSection() const;

        /**
         * @brief Publish a validated montage cursor.
         * @param cursor Cursor from a successful evaluation.
         */
        void Apply(const SAnimationMontageCursor& cursor);

        /**
         * @brief Traverse section links without publishing a new cursor.
         * @param dt Finite nonnegative elapsed seconds.
         * @param output Receives the proposed cursor and visited intervals.
         */
        bool Evaluate(float dt, SAnimationMontageEvaluation& output) const;

        /**
         * @brief Get the action contribution at a cursor.
         * @param cursor Action position and elapsed time.
         */
        float GetWeight(const SAnimationMontageCursor& cursor) const;

        /**
         * @brief Resolve the sequence interval already visited by the action cursor.
         * @param interval Visited action interval with its frame offset.
         */
        SAnimationPlaybackInterval GetPlaybackInterval(
            const SAnimationMontageInterval& interval
        ) const;

        /**
         * @brief Average the action envelope over one visited interval.
         * @param interval Visited chronological interval.
         */
        float GetAverageWeight(const SAnimationMontageInterval& interval) const;

        /**
         * @brief Blend the action onto a continuing pose.
         * @param cursor Action position and elapsed time.
         * @param pose Continuing pose modified on success.
         * @param curves Continuing scalar channels blended with the action.
         * @param referencePose Mesh-specific defaults for sampling.
         */
        bool Sample(
            const SAnimationMontageCursor& cursor,
            SLocalPose& pose,
            AnimationCurveValues& curves,
            const SLocalPose& referencePose
        ) const;

    private:
        /** Owns the montage, committed cursor, section routing, and playback fades. */
        struct SState
        {
            /** Retained immutable montage; null when no action is assigned. */
            Ref<const AnimationMontage> Montage;

            /** Committed sequence position and active playback time. */
            SAnimationMontageCursor Cursor;

            /** Per-section continuation indices; an empty entry ends the route. */
            std::vector<std::optional<size_t>> NextSections;

            /** Elapsed playback seconds when an explicit stop began; -1 means no stop. */
            double StopTime = -1;

            /** Explicit stop fade duration in seconds; used when StopTime is nonnegative. */
            float StopDuration = 0;

            /** Montage contribution captured at StopTime, used as the stop fade's origin. */
            float StopWeight = 0;

            /** Advance the assigned montage; false while paused or unassigned. */
            bool Playing = false;

            /**
             * Visible pose captured when replacing an active montage, used to smooth its
             * replacement's entrance. Empty when blending directly from the continuing pose.
             */
            SLocalPose SourcePose;

            /** Scalar channels captured with an interrupted montage's visible pose. */
            AnimationCurveValues SourceCurves;
        };

        /**
         * @brief Find the elapsed route end without advancing the cursor.
         * @param cursor Starting route position.
         */
        double GetEnd(const SAnimationMontageCursor& cursor) const;

        SState m_State;
    };
}
