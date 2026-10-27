#include "epch.h"
#include "AnimationNode.h"

namespace Elixir
{
    AnimationNode::~AnimationNode() = default;

    bool AnimationNode::CanBind(
        const AnimationParameters& parameters,
        const std::span<const std::string> bindings
    ) const
    {
        if (bindings.size() != GetInputCount())
            return false;

        return std::ranges::all_of(bindings, [&](const auto& name)
        {
            return parameters.GetFloat(name).has_value();
        });
    }

    bool AnimationNode::IsCompatibleWith(const Ref<const Skeleton>& skeleton) const
    {
        const auto& required = GetSkeleton();
        return skeleton && required &&
            (skeleton == required || skeleton->IsCompatible(*required));
    }
}
