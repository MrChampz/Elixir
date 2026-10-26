#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/AnimationNode.h>
#include <Engine/Animation/AnimationNodeEvaluator.h>
#include <Engine/Animation/AnimationInputBindings.h>
#include <Engine/Animation/AnimationPlaybackInterval.h>

namespace Elixir
{
    class AnimationNotifyTracker;
    struct SNotifyEvaluation;

    /** Applies one immutable animation source through its independent node evaluator. */
    class ELIXIR_API AnimationPlayback
    {
    public:
        /** @brief Construct an unassigned playback. */
        AnimationPlayback();

        /**
         * @brief Bind a source and validate its numeric inputs.
         * @param source Immutable animation source retained for the playback lifetime.
         * @param values Numeric coordinates required by the source.
         * @param looping Wrap the playback phase instead of holding its last pose.
         */
        explicit AnimationPlayback(
            Ref<const AnimationNode> source,
            std::span<const float> values = {},
            bool looping = true
        );

        /**
         * @brief Clone independent execution state for a transactional command.
         * @param other Playback to copy.
         */
        AnimationPlayback(const AnimationPlayback& other);

        /**
         * @brief Replace execution state with an independent copy.
         * @param other Playback to copy.
         */
        AnimationPlayback& operator=(const AnimationPlayback& other);

        /**
         * @brief Transfer a playback without cloning its execution state.
         * @param other Playback to consume.
         */
        AnimationPlayback(AnimationPlayback&& other) noexcept;

        /**
         * @brief Transfer execution state without cloning it.
         * @param other Playback to consume.
         */
        AnimationPlayback& operator=(AnimationPlayback&& other) noexcept;

        ~AnimationPlayback();

        /** @brief Check whether a valid source and node evaluator are assigned. */
        bool IsValid() const;

        /** @brief Get the resolved cycle duration in seconds. */
        float GetDuration() const { return GetEvaluationState().Duration; }

        /** @brief Check whether the source can advance or react to live inputs. */
        bool CanAdvance() const;

        /** @brief Get the source position in seconds. */
        float GetTime() const;

        /** @brief Check whether nonlooping playback reached its last pose. */
        bool IsFinished() const;

        /**
         * @brief Replace source inputs without restarting playback.
         * @param values Numeric inputs validated by the concrete source.
         */
        bool SetInputs(std::span<const float> values);

        /**
         * @brief Compute the next normalized phase without changing the current position.
         * @param dt Finite nonnegative elapsed seconds.
         * @param referencePose Mesh-specific sampling defaults.
         */
        double Advance(float dt, const SLocalPose& referencePose = {}) const;

        /**
         * @brief Publish prepared source state after successful frame validation.
         * @param phase Validated next normalized position.
         */
        void Apply(double phase);

        /**
         * @brief Bind character parameters and numeric source input indices.
         * @param parameters Retained character parameter values.
         * @param bindings Resolved input indices.
         */
        bool BindParameters(
            Ref<AnimationParameters> parameters,
            std::span<const size_t> bindings = {}
        );

        /** @brief Check whether the concrete source has an internal transition. */
        bool IsTransitioning() const;

        /**
         * @brief Convert a seek time using the concrete source's cyclic policy.
         * @param seconds Finite destination time in seconds.
         */
        double GetSeekPhase(float seconds) const;

        /**
         * @brief Seek a source without publishing timeline events.
         * @param phase Normalized destination position.
         */
        bool SeekPhase(double phase);

        /** @brief Get the active state name for a machine source. */
        std::string_view GetCurrentState() const;

        /**
         * @brief Sample a local pose at a normalized phase.
         * @param phase Source phase in [0, 1].
         * @param referencePose Mesh-specific defaults.
         * @param output Receives the complete local pose.
         */
        bool Sample(
            double phase,
            const SLocalPose& referencePose,
            SLocalPose& output
        ) const;

        /**
         * @brief Resolve the notification interval without selecting source types.
         * @param dt Elapsed source seconds.
         * @param offset Frame seconds preceding this interval.
         */
        SAnimationPlaybackInterval GetPlaybackInterval(double dt, double offset = 0) const;

        /**
         * @brief Collect selected events from this source, including composed children.
         * @param offset Seconds from the start of the prepared frame.
         * @param dt Elapsed source seconds.
         * @param layerName Enclosing control name.
         * @param tracker Enclosing lifecycle tracker.
         * @param output Candidate events and activations.
         */
        bool EvaluateNotifies(
            double dt,
            std::string_view layerName,
            const AnimationNotifyTracker& tracker,
            SNotifyEvaluation& output,
            double offset = 0
        ) const;

        /**
         * @brief Find a control inside a composed source.
         * @param name Control name.
         */
        AnimationLayer* GetLayer(std::string_view name) const;

        /** @brief Count controls exposed by the assigned source. */
        size_t GetLayerCount() const;

        /**
         * @brief Enable notification tracking in nested controls for the next evaluation.
         * @param enabled Whether the enclosing composition can publish source events.
         */
        void SetNotifiesEnabled(bool enabled) const;

        /**
         * @brief Extract root movement over an interval of the current playback.
         * @param interval Root reference transform, bone index, duration, and offset to
         * extract.
         * @param output Receives the extracted movement on success; unchanged on failure.
         * @return True if movement was extracted successfully.
         */
        bool ExtractRootMotion(
            const SRootMotionIntervalContext& interval,
            SRootMotionDelta& output
        ) const;

        /** @brief Get the normalized source position in [0, 1]. */
        double GetPhase() const { return m_Phase; }

        /** @brief Check whether cyclic playback repeats. */
        bool IsLooping() const { return m_Looping; }

        /** @brief Suppress point notifications exactly at the current position. */
        void SuppressInitialNotifies() { m_IncludeNotifyStart = false; }

        /** @brief Describe the current node's resolved contributions and child controls. */
        SAnimationNodeState GetEvaluationState() const;

    private:
        double m_Phase = 0;
        bool m_Looping = true;
        bool m_IncludeNotifyStart = true;
        mutable bool m_PositiveAdvance = false;

        Scope<AnimationNodeEvaluator> m_Evaluator;
        mutable SAnimationEvaluation m_Evaluation;
        Ref<const AnimationNode> m_Node;
        mutable AnimationInputBindings m_Inputs;

        mutable bool m_Prepared = false;
        mutable bool m_NotifiesEnabled = true;
    };
}
