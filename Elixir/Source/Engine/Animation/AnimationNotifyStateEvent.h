#pragma once

#include <Engine/Animation/AnimationSequence.h>

namespace Elixir
{
    /** Selects the lifecycle step of an active animation window. */
    enum class EAnimationNotifyStateEventType : uint8_t
    {
        Begin,
        Update,
        End
    };

    /** Retains one window lifecycle event produced by a successful instance update. */
    struct SAnimationNotifyStateEvent
    {
        /** Shared ownership retains window metadata and its payload after playback changes. */
        Ref<const AnimationSequence> Sequence;

        /** Index into Sequence's immutable GetNotifyStates list. */
        size_t NotifyStateIndex = 0;

        /** Layer-local activation identity, shared by its Begin, Update, and End. */
        uint64_t ActivationId = 0;
        EAnimationNotifyStateEventType Type = EAnimationNotifyStateEventType::Begin;

        /** Seconds from this frame's start; Update is timestamped at its overlap end. */
        double TimeOffset = 0.0;

        /** Active frame seconds covered by Update; zero for Begin and End. */
        double DeltaSeconds = 0.0;
        /** Owning layer name, including Base; retained after layer removal. */
        std::string LayerName;

        /** End was forced by a playback change, jump, mute, stop, or layer removal. */
        bool Cancelled = false;

        /**
         * @brief Read immutable metadata and its typed payload.
         * @pre Sequence is non-null and NotifyStateIndex refers to one of its windows.
         */
        const SAnimationNotifyState& GetNotifyState() const
        {
            return Sequence->GetNotifyStates()[NotifyStateIndex];
        }
    };
}
