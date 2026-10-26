#pragma once

#include <Engine/Animation/AnimationNotifyEvent.h>
#include <Engine/Animation/AnimationTraversal.h>

namespace Elixir
{
    class AnimationPlayback;

    /** Tracks one active notification with duration until it ends or is cancelled. */
    struct SActiveNotify
    {
        /** Retained sequence that owns the notification definition and payload. */
        Ref<const AnimationSequence> Sequence;

        /** Index of the notification in Sequence->GetNotifies(). */
        size_t Index = 0;

        /** Nonzero identifier shared by this activation's Begin, Update, and End events. */
        uint64_t ActivationId = 0;

        /** Owning layer name or qualified path, preserved through cancellation. */
        std::string LayerName;
    };

    /** Candidate lifecycle state published only after a complete animation frame succeeds. */
    struct SNotifyEvaluation
    {
        std::vector<SActiveNotify> Active;
        std::vector<SAnimationNotifyEvent> Events;
        uint64_t NextActivationId = 1;
    };

    /** Owns notification activation identities, lifetimes, and pending cancellation events. */
    class ELIXIR_API AnimationNotifyTracker
    {
    public:
        /**
         * @brief Queue cancellation events without publishing a new frame.
         * @param layerName Name retained by the generated events.
         */
        void Cancel(std::string_view layerName);

        /**
         * @brief Initialize candidate state from the last committed frame.
         * @param output Receives active notifications and queued cancellations.
         */
        void Begin(SNotifyEvaluation& output) const;

        /**
         * @brief Evaluate a resolved interval without inspecting its source type.
         * @param traversal Resolved sequence interval and timing policy.
         * @param layerName Name retained by emitted events.
         * @param output Candidate activation state to advance.
         * @param allowActivation Allow notifications with duration to begin or update.
         */
        static bool Evaluate(
            const SAnimationTraversal& traversal,
            std::string_view layerName,
            SNotifyEvaluation& output,
            bool allowActivation = true
        );

        /**
         * @brief Process resolved sources without asking an evaluator to emit notifications.
         * @param playback Owner of the source interval and its clock.
         * @param dt Length of the interval in seconds.
         * @param layerName Enclosing control name.
         * @param output Candidate notification events and activations.
         * @param offset Seconds from the start of the candidate frame.
         */
        static bool EvaluatePlayback(
            const AnimationPlayback& playback,
            double dt,
            std::string_view layerName,
            SNotifyEvaluation& output,
            double offset = 0
        );

        /**
         * @brief End every candidate activation at a timeline boundary.
         * @param output Candidate state receiving End events.
         * @param offset Event time in seconds within the frame.
         * @param cancelled Whether playback was interrupted.
         * @param layerName Name retained by emitted events.
         */
        static void End(
            SNotifyEvaluation& output,
            double offset,
            bool cancelled,
            std::string_view layerName
        );

        /**
         * @brief Publish validated activation state and consume pending cancellations.
         * @param output Candidate state from a successful frame.
         */
        void Apply(SNotifyEvaluation output);

        /** @brief Inspect queued cancellation events without consuming them. */
        const std::vector<SAnimationNotifyEvent>& GetPendingEnds() const { return m_Pending; }

    private:
        std::vector<SActiveNotify> m_Active;
        std::vector<SAnimationNotifyEvent> m_Pending;
        uint64_t m_NextActivationId = 1;
    };
}
