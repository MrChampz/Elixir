#include "epch.h"
#include "StateMachineNode.h"
#include "StateMachineEvaluator.h"

namespace Elixir
{
    Ref<StateMachineNode> StateMachineNode::Create(Ref<const AnimationStateMachine> resource)
    {
        if (!resource || !resource->GetSkeleton())
        {
            EE_CORE_ERROR("StateMachineNode requires a valid AnimationStateMachine resource.")
            return nullptr;
        }

        return Ref<StateMachineNode>(new StateMachineNode(std::move(resource)));
    }

    StateMachineNode::StateMachineNode(Ref<const AnimationStateMachine> resource)
        : m_Resource(std::move(resource)) {}

    Scope<AnimationNodeEvaluator> StateMachineNode::CreateEvaluator() const
    {
        return CreateScope<StateMachineEvaluator>(*m_Resource);
    }

    bool StateMachineNode::CanBind(
        const AnimationParameters& parameters,
        const std::span<const std::string> bindings
    ) const
    {
        return m_Resource->CanBind(parameters, bindings);
    }
}
