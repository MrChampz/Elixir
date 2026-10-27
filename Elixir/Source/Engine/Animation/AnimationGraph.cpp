#include "epch.h"
#include "AnimationGraph.h"

namespace Elixir
{
    Ref<AnimationGraph> AnimationGraph::Create(SAnimationGraphCreateInfo info)
    {
        const AnimationParameters parameters(info.Parameters);

        if (info.Name.empty() || !parameters.IsValid() || !info.Output)
        {
            EE_CORE_ERROR("Animation graph requires a name, valid parameters, and an output.")
            return nullptr;
        }

        auto root = BlendNode::Create(
            info.Name,
            {{
                .Node = info.Output,
                .InputParameters = info.InputParameters,
                .Looping = info.Looping
            }}
        );

        if (!root || !root->CanBind(parameters, {}))
        {
            EE_CORE_ERROR("Animation graph output or its parameter bindings are invalid.")
            return nullptr;
        }

        return Ref<AnimationGraph>(new AnimationGraph(
            std::move(info),
            std::move(root)
        ));
    }

    AnimationGraph::AnimationGraph(
        SAnimationGraphCreateInfo info,
        Ref<const BlendNode> root
    ) : m_Info(std::move(info)),
        m_Root(std::move(root)) {}

    Ref<Animation> AnimationGraph::CreateAnimation(
        const SLocalPose& referencePose
    ) const
    {
        auto animation = Animation::Create(GetSkeleton(), referencePose);

        if (!animation ||
            !animation->SetParameters(m_Info.Parameters) ||
            !animation->Play(m_Root))
            return nullptr;

        return animation;
    }

    const Ref<const Skeleton>& AnimationGraph::GetSkeleton() const
    {
        return m_Root->GetSkeleton();
    }
}
