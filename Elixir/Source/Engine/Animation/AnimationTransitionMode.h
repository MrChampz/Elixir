#pragma once

#include <cstdint>

namespace Elixir
{
    /** Selects how playback changes converge to the destination animation. */
    enum class EAnimationTransitionMode : uint8_t
    {
        CrossFade,
        Inertialization
    };
}
