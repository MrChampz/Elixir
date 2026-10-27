#include "epch.h"
#include "AnimationNotifyEvent.h"

namespace Elixir
{
    std::string_view SAnimationNotifyEvent::GetName() const
    {
        return Sequence->GetNotifies()[NotifyIndex].Name;
    }
}
