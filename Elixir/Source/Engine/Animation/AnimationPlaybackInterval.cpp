#include "epch.h"
#include "AnimationPlaybackInterval.h"

namespace Elixir
{
    bool SAnimationPlaybackInterval::IsValid() const
    {
        return Sequence &&
            std::isfinite(StartPhase) &&
            std::isfinite(EndPhase) &&
            EndPhase >= StartPhase &&
            std::isfinite(DurationSeconds) &&
            DurationSeconds >= 0 &&
            std::isfinite(FrameOffset) &&
            FrameOffset >= 0;
    }
}
