#pragma once

#include <Engine/Animation/Sequence.h>

namespace Elixir
{
    /** Identifies an instantaneous notification or a notification's lifecycle step. */
    enum class EAnimationNotifyEventType : uint8_t
    {
        Instant,
        Begin,
        Update,
        End
    };

    /** Retains one notification emitted by a successful animation update. */
    struct ELIXIR_API SAnimationNotifyEvent
    {
        /** Shared ownership keeps notification metadata and its payload alive. */
        Ref<const Sequence> Sequence;

        /** Index into the sequence's immutable GetNotifies list. */
        size_t NotifyIndex = 0;

        /** Layer-local window identity; zero for instantaneous notifications. */
        uint64_t ActivationId = 0;

        EAnimationNotifyEventType Type = EAnimationNotifyEventType::Instant;

        /** Seconds from this update's start; Update is timestamped at its overlap end. */
        double TimeOffset = 0.0;

        /** Active seconds covered by Update; zero for all other event types. */
        double DeltaSeconds = 0.0;

        /** Owning layer name, including Base; retained after layer removal. */
        std::string LayerName;

        /** True when End was caused by an interruption instead of timeline completion. */
        bool Cancelled = false;

        /**
         * @brief Get the authored notification name.
         * @pre Sequence is non-null and NotifyIndex refers to one of its notifications.
         */
        std::string_view GetName() const;

        /**
         * @brief Read the retained payload without conversion or exceptions.
         * @tparam T Exact stored copyable value type.
         * @return A borrowed read-only value, or null for a different or absent type.
         * @pre Sequence is non-null and NotifyIndex refers to one of its notifications.
         */
        template<typename T>
        const T* GetPayload() const
        {
            return Sequence->GetNotifies()[NotifyIndex].GetPayload<T>();
        }
    };
}
