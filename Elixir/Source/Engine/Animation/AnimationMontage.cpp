#include "epch.h"
#include "AnimationMontage.h"

namespace Elixir
{
    Ref<AnimationMontage> AnimationMontage::Create(SAnimationMontageCreateInfo info)
    {
        if (info.Name.empty() ||
            !info.Sequence ||
            info.Sequence->GetDuration() <= 0 ||
            !std::isfinite(info.BlendIn) ||
            info.BlendIn < 0 ||
            !std::isfinite(info.BlendOut) ||
            info.BlendOut < 0)
        {
            EE_CORE_ERROR("Animation montage requires a name, a timed sequence, and valid fades.")
            return nullptr;
        }

        return Ref<AnimationMontage>(new AnimationMontage(std::move(info)));
    }

    AnimationMontage::AnimationMontage(SAnimationMontageCreateInfo info)
        : m_Info(std::move(info)) {}
}
