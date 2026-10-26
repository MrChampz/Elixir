#include "epch.h"
#include "BlendSpaceNode.h"

#include <Engine/Animation/BlendSpaceEvaluator.h>

namespace Elixir
{
    Ref<BlendSpaceNode> BlendSpaceNode::Create(Ref<const BlendSpace> resource)
    {
        if (!resource || !resource->GetSkeleton())
        {
            EE_CORE_ERROR("BlendSpaceNode requires a valid BlendSpace resource.")
            return nullptr;
        }
        return Ref<BlendSpaceNode>(new BlendSpaceNode(std::move(resource)));
    }

    Scope<AnimationNodeEvaluator> BlendSpaceNode::CreateEvaluator() const
    {
        return CreateScope<BlendSpaceEvaluator>(*m_Resource);
    }

    const Ref<const Skeleton>& BlendSpaceNode::GetSkeleton() const
    {
        return m_Resource->GetSkeleton();
    }

    size_t BlendSpaceNode::GetInputCount() const
    {
        return m_Resource->GetDimensionCount();
    }


    BlendSpaceNode::BlendSpaceNode(Ref<const BlendSpace> resource)
        : m_Resource(std::move(resource)) {}
}
