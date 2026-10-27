#include <gtest/gtest.h>
#include <Engine/Animation/AnimationGraph.h>
#include <Engine/Animation/SequenceNode.h>
#include <Engine/Animation/BlendSpaceNode.h>
#include <Engine/Animation/StateMachineNode.h>
#include <Engine/Animation/BlendSpace1D.h>
#include "../Mesh/SkinningTestData.h"

using namespace Elixir;

TEST(AnimationNodeContractTest, ResourcesAndGraphsCannotBeConnectedAsNodes)
{
    static_assert(!std::is_base_of_v<AnimationNode, AnimationGraph>);
    static_assert(!std::is_base_of_v<AnimationNode, Sequence>);
    static_assert(!std::is_base_of_v<AnimationNode, BlendSpace>);
    static_assert(!std::is_base_of_v<AnimationNode, StateMachine>);
    static_assert(!std::is_convertible_v<Ref<const AnimationGraph>, Ref<const AnimationNode>>);
    static_assert(std::is_base_of_v<AnimationNode, SequenceNode>);
    static_assert(std::is_base_of_v<AnimationNode, BlendSpaceNode>);
    static_assert(std::is_base_of_v<AnimationNode, StateMachineNode>);
    static_assert(std::is_base_of_v<AnimationNode, BlendNode>);
    SUCCEED();
}

TEST(AnimationNodeContractTest, NodeRetainsItsResourceForTheAnimationLifetime)
{
    const auto mesh = SkinningTests::MakeMesh();
    auto sequence = Sequence::Create("Clip", mesh->GetSkeleton(), 1, {});
    const std::weak_ptr<const Sequence> resource = sequence;
    auto node = SequenceNode::Create(sequence);
    ASSERT_TRUE(node);
    EXPECT_EQ(node->GetSequence(), sequence);
    sequence.reset();
    EXPECT_FALSE(resource.expired());
    auto graph = AnimationGraph::Create({.Name = "Character", .Output = node});
    ASSERT_TRUE(graph);
    const auto animation = graph->CreateAnimation(mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    graph.reset();
    node.reset();
    ASSERT_TRUE(animation->Update(0.25f));
    EXPECT_FALSE(resource.expired());
    animation->Stop();
    EXPECT_TRUE(resource.expired());
}

TEST(AnimationNodeContractTest, OneGraphConnectsResourceNodesAndComposesTheirPoses)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto first = Sequence::Create("First", mesh->GetSkeleton(), 1, {
        {.BoneIndex = 0, .Translations = {{0, {1, 0, 0}}}}
    });
    const auto second = Sequence::Create("Second", mesh->GetSkeleton(), 1, {
        {.BoneIndex = 0, .Translations = {{0, {5, 0, 0}}}}
    });
    const auto space = BlendSpace1D::Create("Movement", {{0, first}, {1, second}});
    const auto machine = StateMachine::Create({
        .InitialState = "Idle",
        .States = {{"Idle", SequenceNode::Create(first)},
            {"Attack", SequenceNode::Create(second)}},
        .Transitions = {{.From = "Idle", .To = "Attack",
            .Conditions = {{"Attack", EAnimationComparison::Equal, true}}, .FadeDuration = 0}}
    });
    ASSERT_TRUE(machine);
    const auto output = BlendNode::Create("Composition", {
        {.Name = "Movement", .Node = BlendSpaceNode::Create(space), .InputParameters = {"Speed"}},
        {.Name = "Action", .Mask = BoneMask::Create("Full", mesh->GetSkeleton(), {1, 1}),
            .Weight = 0.5f, .Node = StateMachineNode::Create(machine)}
    });
    const auto graph = AnimationGraph::Create({
        .Name = "Character",
        .Parameters = {{"Speed", 0.5f}, {"Attack", false}},
        .Output = output
    });
    ASSERT_TRUE(graph);
    EXPECT_EQ(graph->GetOutput(), output);
    const auto animation = graph->CreateAnimation(mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    EXPECT_FLOAT_EQ(animation->GetLocalPose().LocalTransforms[0].Translation.x, 2);
    ASSERT_TRUE(animation->GetParameters().SetBool("Attack", true));
    ASSERT_TRUE(animation->Update(0.25f));
    EXPECT_FLOAT_EQ(animation->GetLocalPose().LocalTransforms[0].Translation.x, 4);
    EXPECT_EQ(animation->GetCurrentState("Action"), "Attack");
}

TEST(AnimationNodeContractTest, RejectsMissingResourcesAndMissingOutputNodes)
{
    EXPECT_FALSE(SequenceNode::Create(nullptr));
    EXPECT_FALSE(BlendSpaceNode::Create(nullptr));
    EXPECT_FALSE(StateMachineNode::Create(nullptr));
    EXPECT_FALSE(AnimationGraph::Create({.Name = "MissingOutput"}));
}
