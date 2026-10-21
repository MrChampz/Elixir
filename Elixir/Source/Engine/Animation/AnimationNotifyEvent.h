#pragma once

#include <Engine/Animation/AnimationSequence.h>

namespace Elixir
{
    /** Retains the authored sequence for one event emitted by a successful update. */
    struct SAnimationNotifyEvent
    {
        /** Shared ownership keeps event metadata alive after playback changes. */
        Ref<const AnimationSequence> Sequence;

        /** Index into Sequence's sorted notify list. */
        size_t NotifyIndex = 0;

        /** Seconds elapsed within this update when playback crossed the event. */
        double TimeOffset = 0.0;

        /** Owning layer name, including Base for base playback. Retained with the event. */
        std::string LayerName;

        /**
         * @brief Get the immutable event metadata retained by this occurrence.
         * @pre Sequence is non-null and NotifyIndex refers to one of its notifies.
         */
        const SAnimationNotify& GetNotify() const
        {
            return Sequence->GetNotifies()[NotifyIndex];
        }
    };

}
