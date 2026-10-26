#include "epch.h"
#include "SequenceNode.h"

#include <Engine/Animation/SequenceEvaluator.h>

namespace Elixir
{
    Ref<SequenceNode> SequenceNode::Create(Ref<const AnimationSequence> resource)
    {
        if (!resource || !resource->GetSkeleton())
        {
            EE_CORE_ERROR("SequenceNode requires a valid AnimationSequence resource.")
            return nullptr;
        }

        return Ref<SequenceNode>(new SequenceNode(std::move(resource)));
    }

    SequenceNode::SequenceNode(Ref<const AnimationSequence> resource)
        : m_Resource(std::move(resource)) {}

    Scope<AnimationNodeEvaluator> SequenceNode::CreateEvaluator() const
    {
        return CreateScope<SequenceEvaluator>(*m_Resource);
    }
}
