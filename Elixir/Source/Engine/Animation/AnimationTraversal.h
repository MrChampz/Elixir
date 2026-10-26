#pragma once

#include <Engine/Animation/AnimationSequence.h>

namespace Elixir
{
    /** Describes a visited sequence interval on the playback reference timeline. */
    struct ELIXIR_API SAnimationTraversal
    {
        /** Retained sequence whose notifications are evaluated. */
        Ref<const AnimationSequence> Sequence;

        /** Retained marker reference for synchronization; null uses normalized sequence time. */
        Ref<const AnimationSequence> Reference;

        /** Finite starting phase on the reference timeline; whole units represent cycles. */
        double StartPhase = 0;

        /** Finite ending phase, not less than StartPhase; may cross multiple cycles. */
        double EndPhase = 0;

        /** Effective playback cycle duration in seconds, used to convert phase offsets to time. */
        double DurationSeconds = 0;

        /**
         * Finite nonnegative offset of this interval's start within the current frame, in
         * seconds.
         */
        double FrameOffset = 0;

        /** Allow the interval to cross sequence cycle boundaries. */
        bool Looping = true;

        /** Include notifications exactly at the starting phase. */
        bool IncludeStart = false;

        /** Cancel active notifications before processing a discontinuous source change. */
        bool ResetActivations = false;

        /**
         * @brief Check the retained sequence and finite forward timeline values.
         * @return True when the sequence exists, phases are ordered, and timing values are
         * valid.
         */
        bool IsValid() const;
    };
}
