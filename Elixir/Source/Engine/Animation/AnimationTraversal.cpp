#include "epch.h"
#include "AnimationTraversal.h"

namespace Elixir
{
    bool SAnimationTraversal::IsValid() const
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
