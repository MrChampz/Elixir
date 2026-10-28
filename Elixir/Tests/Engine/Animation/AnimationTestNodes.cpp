#include "AnimationTestNodes.h"

using namespace Elixir;

Ref<const AnimationNode> MakeNode(Ref<const Sequence> sequence)
{
    return SequenceNode::Create(std::move(sequence));
}

Ref<const AnimationNode> MakeNode(Ref<const BlendSpace> space)
{
    return BlendSpaceNode::Create(std::move(space));
}

Ref<const AnimationNode> MakeNode(Ref<const StateMachine> machine)
{
    return StateMachineNode::Create(std::move(machine));
}

Ref<const AnimationNode> MakeNode(Ref<const AnimationNode> node)
{
    return node;
}

Ref<const AnimationNode> MakeNode(std::nullptr_t value)
{
    return nullptr;
}
