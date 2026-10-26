#include "epch.h"
#include "BlendNode.h"

#include <Engine/Animation/BlendNodeEvaluator.h>

namespace Elixir
{
    /* SBlendNodeInput */

    bool SBlendNodeInput::IsValid(
        const Ref<const Skeleton>& skeleton,
        const AnimationParameters& parameters,
        const bool first
    ) const
    {
        return IsValidSettings(skeleton, first) && Node->CanBind(parameters, InputParameters);
    }

    bool SBlendNodeInput::IsValidSettings(
        const Ref<const Skeleton>& skeleton,
        const bool first
    ) const
    {
        if (Name.empty() ||
            Name.find('/') != std::string::npos ||
            !Node ||
            !Node->IsCompatibleWith(skeleton))
            return false;

        if (!std::isfinite(Weight) || Weight < 0 || Weight > 1)
            return false;

        if (first)
        {
            if (Mask ||
                Weight != 1 ||
                BlendMode != EAnimationLayerBlendMode::Override ||
                !AdditiveReferencePose.LocalTransforms.empty())
                return false;
        }
        else
        {
            if (!Mask) return false;

            const auto& maskSkeleton = Mask->GetSkeleton();
            if (maskSkeleton != skeleton && !skeleton->IsCompatible(*maskSkeleton))
                return false;
        }

        if (!AdditiveReferencePose.LocalTransforms.empty() &&
            !AdditiveReferencePose.IsValid(skeleton->GetBones().size()))
            return false;
        
        return Node->IsValidPlayback(Looping) &&
            InputParameters.size() == Node->GetInputCount();
    }

    /* BlendNode */

    Ref<BlendNode> BlendNode::Create(
        std::string name,
        std::vector<SBlendNodeInput> inputs
    )
    {
        if (name.empty() || inputs.empty() || !inputs.front().Node)
        {
            EE_CORE_ERROR("Animation blend requires a name and a connected base source.")
            return nullptr;
        }

        const auto skeleton = inputs.front().Node->GetSkeleton();
        if (!skeleton)
        {
            EE_CORE_ERROR("Animation blend base source requires a valid skeleton.")
            return nullptr;
        }

        std::unordered_set<std::string> names;
        for (size_t i = 0; i < inputs.size(); ++i)
        {
            const auto& input = inputs[i];

            if (names.contains(input.Name))
            {
                EE_CORE_ERROR("Animation blend repeats input '{}'.", input.Name)
                return nullptr;
            }

            if (!input.IsValidSettings(skeleton, i == 0))
            {
                EE_CORE_ERROR("Animation blend input '{}' has invalid settings.", input.Name)
                return nullptr;
            }

            names.insert(input.Name);
        }

        return Ref<BlendNode>(new BlendNode(
            std::move(name),
            std::move(inputs),
            skeleton
        ));
    }

    Scope<AnimationEvaluator> BlendNode::CreateEvaluator() const
    {
        return CreateScope<BlendNodeEvaluator>(*this);
    }

    bool BlendNode::CanBind(
        const AnimationParameters& parameters,
        const std::span<const std::string> bindings
    ) const
    {
        if (!bindings.empty()) return false;

        for (size_t i = 0; i < m_Inputs.size(); ++i)
            if (!m_Inputs[i].IsValid(m_Skeleton, parameters, i == 0))
                return false;

        return true;
    }

    BlendNode::BlendNode(
        std::string name,
        std::vector<SBlendNodeInput> inputs,
        Ref<const Skeleton> skeleton
    ) : m_Name(std::move(name)),
        m_Inputs(std::move(inputs)),
        m_Skeleton(std::move(skeleton)) {}
}
