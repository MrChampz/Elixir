#include "AnimationTestNodes.h"
#include <gtest/gtest.h>
#include <Engine/Animation/AnimationGraph.h>
#include <Engine/Animation/AnimationStateMachine.h>
#include <Engine/Animation/BlendSpace1D.h>
#include <Engine/Mesh/SkeletalMeshInstance.h>
#include "../Mesh/SkinningTestData.h"

#include <unordered_map>

using namespace Elixir;

namespace
{
    /**
     * @brief Create a constant root translation for composition tests.
     * @param mesh Mesh whose skeleton is retained by the clip.
     * @param value Constant root translation on X.
     */
    Ref<AnimationSequence> Clip(const Ref<SkeletalMesh>& mesh, const float value)
    {
        return AnimationSequence::Create("Clip", mesh->GetSkeleton(), 1, {
            {.BoneIndex = 0, .Translations = {{0, {value, 0, 0}}}}
        });
    }

    /**
     * @brief Build a full-body mask used by an explicit blend node.
     * @param mesh Mesh whose skeleton defines the mask.
     */
    Ref<BoneMask> Mask(const Ref<SkeletalMesh>& mesh)
    {
        return BoneMask::Create("Full", mesh->GetSkeleton(), {1, 1});
    }
}

TEST(AnimationGraphCompositionTest, SequenceAndBlendSpaceConnectDirectlyToTheOutput)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto first = Clip(mesh, 0);
    const auto second = Clip(mesh, 4);
    const auto direct = AnimationGraph::Create({.Name = "Direct", .Output = MakeNode(second)});
    ASSERT_TRUE(direct);
    const auto animation = direct->CreateAnimation(mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    EXPECT_FLOAT_EQ(animation->GetLocalPose().LocalTransforms[0].Translation.x, 4);

    const auto space = BlendSpace1D::Create("Movement", {{0, first}, {1, second}});
    const auto graph = AnimationGraph::Create({.Name = "Space",
        .Parameters = {{"Speed", 0.5f}}, .Output = MakeNode(space), .InputParameters = {"Speed"}});
    ASSERT_TRUE(graph);
    const auto movement = graph->CreateAnimation(mesh->GetData().ReferencePose);
    ASSERT_TRUE(movement);
    EXPECT_FLOAT_EQ(movement->GetLocalPose().LocalTransforms[0].Translation.x, 2);
    ASSERT_TRUE(movement->GetParameters().SetFloat("Speed", 1));
    ASSERT_TRUE(movement->Update(0.25f));
    EXPECT_FLOAT_EQ(movement->GetLocalPose().LocalTransforms[0].Translation.x, 4);
}

TEST(AnimationGraphCompositionTest, NestedNodesComposeTheirOutputsInsteadOfFlatteningInputs)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto inner = BlendNode::Create("Inner", {
        {.Name = "First", .Node = MakeNode(Clip(mesh, 0))},
        {.Name = "Second", .Mask = Mask(mesh), .Weight = 0.5f, .Node = MakeNode(Clip(mesh, 4))}
    });
    const auto outer = BlendNode::Create("Outer", {
        {.Name = "Inner", .Node = MakeNode(inner)},
        {.Name = "Third", .Mask = Mask(mesh), .Weight = 0.25f, .Node = MakeNode(Clip(mesh, 8))}
    });
    const auto graph = AnimationGraph::Create({.Name = "Nested", .Output = MakeNode(outer)});
    ASSERT_TRUE(graph);
    const auto animation = graph->CreateAnimation(mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    EXPECT_FLOAT_EQ(animation->GetLocalPose().LocalTransforms[0].Translation.x, 3.5f);
    ASSERT_TRUE(animation->GetLayer("Second"));
    ASSERT_TRUE(animation->GetLayer("Second")->SetWeight(1));
    ASSERT_TRUE(animation->Update(0.25f));
    EXPECT_FLOAT_EQ(animation->GetLocalPose().LocalTransforms[0].Translation.x, 5);
}

TEST(AnimationGraphCompositionTest, FailedCompositionPreservesNestedMachineDecisionsAndClocks)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto machine = AnimationStateMachine::Create({.InitialState = "First",
        .States = {{"First", MakeNode(Clip(mesh, 0))}, {"Second", MakeNode(Clip(mesh, 4))}},
        .Transitions = {{.From = "First", .To = "Second",
            .Conditions = {{"Switch", EAnimationComparison::Equal, true}}, .FadeDuration = 0}}
    });
    const auto mirrored = AnimationSequence::Create("Mirror", mesh->GetSkeleton(), 1, {
        {.BoneIndex = 0, .Scales = {{0, {-1, 1, 1}}}}
    });
    const auto blend = BlendNode::Create("Composition", {
        {.Node = MakeNode(machine)},
        {.Name = "Overlay", .Mask = Mask(mesh), .Weight = 0, .Node = MakeNode(mirrored)}
    });
    const auto graph = AnimationGraph::Create({.Name = "Transactional",
        .Parameters = {{"Switch", false}}, .Output = MakeNode(blend)});
    ASSERT_TRUE(graph);
    const auto animation = graph->CreateAnimation(mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    ASSERT_TRUE(animation->Update(0.25f));
    ASSERT_TRUE(animation->GetLayer("Overlay")->SetWeight(0.5f));
    ASSERT_TRUE(animation->GetParameters().SetBool("Switch", true));
    EXPECT_FALSE(animation->Update(0.25f));
    EXPECT_EQ(animation->GetCurrentState(), "First");
    EXPECT_FLOAT_EQ(animation->GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(animation->GetLocalPose().LocalTransforms[0].Translation.x, 0);
    ASSERT_TRUE(animation->GetLayer("Overlay")->SetWeight(0));
    ASSERT_TRUE(animation->Update(0.25f));
    EXPECT_EQ(animation->GetCurrentState(), "Second");
}

TEST(AnimationGraphCompositionTest, ComposedSourcesEmitAndCancelAllNamedNotifyWindows)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto clip = Clip(mesh, 0)->WithNotifies({{"Window", 0, 1}});
    const auto blend = BlendNode::Create("TwoSources", {
        {.Name = "Locomotion", .Node = MakeNode(clip)},
        {.Name = "Action", .Mask = Mask(mesh), .Node = MakeNode(clip)}
    });
    const auto machine = AnimationStateMachine::Create({.InitialState = "Composed",
        .States = {{"Composed", MakeNode(blend)}}});
    const auto graph = AnimationGraph::Create({.Name = "Events", .Output = MakeNode(machine)});
    ASSERT_TRUE(graph);
    const auto animation = graph->CreateAnimation(mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    ASSERT_TRUE(animation->Update(0.25f));
    ASSERT_EQ(animation->GetNotifyEvents().size(), 4);
    EXPECT_EQ(std::ranges::count_if(animation->GetNotifyEvents(), [](const auto& event)
        { return event.LayerName == "Action"; }), 2);
    ASSERT_TRUE(animation->Play(MakeNode(Clip(mesh, 0))));
    ASSERT_TRUE(animation->Update(0));
    ASSERT_EQ(animation->GetNotifyEvents().size(), 2);
    for (const auto& event : animation->GetNotifyEvents())
    {
        EXPECT_EQ(event.Type, EAnimationNotifyEventType::End);
        EXPECT_TRUE(event.Cancelled);
        EXPECT_TRUE(event.LayerName == "Action" || event.LayerName == "Locomotion");
    }
}

TEST(AnimationGraphCompositionTest, OneGraphCreatesIndependentExecutionWithoutBecomingANode)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto clip = Clip(mesh, 2);
    const auto outer = AnimationGraph::Create({.Name = "Graph", .Output = MakeNode(clip)});
    ASSERT_TRUE(outer);
    const auto first = outer->CreateAnimation(mesh->GetData().ReferencePose);
    const auto second = outer->CreateAnimation(mesh->GetData().ReferencePose);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(first->Update(0.25f));
    EXPECT_FLOAT_EQ(first->GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(second->GetTime(), 0);
    const auto standalone = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(standalone->Play(MakeNode(outer->GetOutput())));
    ASSERT_TRUE(standalone->Update(0.5f));
    EXPECT_FLOAT_EQ(standalone->GetLocalPose().LocalTransforms[0].Translation.x, 2);
}

TEST(AnimationGraphCompositionTest, MeshAndAnimationHaveIndependentLifetimesAndPoseOwnership)
{
    EXPECT_FALSE(SkeletalMeshInstance::Create(nullptr));
    auto mesh = SkinningTests::MakeMesh();
    auto instance = SkeletalMeshInstance::Create(mesh);
    ASSERT_TRUE(instance);
    const auto graph = AnimationGraph::Create({.Name = "Independent", .Output = MakeNode(Clip(mesh, 5))});
    const auto animation = graph->CreateAnimation(mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    const auto reference = instance->GetReferenceModelPose();
    ASSERT_TRUE(animation->Update(0.25f));
    EXPECT_EQ(instance->GetReferenceModelPose().Matrices, reference.Matrices);
    EXPECT_NE(animation->GetModelPose().Matrices[0], reference.Matrices[0]);
    instance.reset();
    mesh.reset();
    EXPECT_TRUE(animation->Update(0.25f));
}

TEST(AnimationGraphCompositionTest, ReusedNodesExposeIndependentControlsByQualifiedPath)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto clip = Clip(mesh, 0)->WithNotifies({{"Window", 0, 1}});
    const auto branch = BlendNode::Create("Branch", {
        {.Node = MakeNode(Clip(mesh, 0))},
        {.Name = "Action", .Mask = Mask(mesh), .Node = MakeNode(clip)}
    });
    const auto composition = BlendNode::Create("Composition", {
        {.Name = "First", .Node = MakeNode(branch)},
        {.Name = "Second", .Mask = Mask(mesh), .Node = MakeNode(branch)}
    });
    const auto graph = AnimationGraph::Create({.Name = "Shared", .Output = MakeNode(composition)});
    ASSERT_TRUE(graph);
    const auto animation = graph->CreateAnimation(mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    EXPECT_FALSE(animation->GetLayer("Action"));
    const auto first = animation->GetLayer("First/Action");
    const auto second = animation->GetLayer("Second/Action");
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_NE(first, second);
    ASSERT_TRUE(first->Seek(0.5f));
    EXPECT_FLOAT_EQ(second->GetTime(), 0);
    ASSERT_TRUE(animation->Update(0.1f));
    EXPECT_EQ(std::ranges::count_if(animation->GetNotifyEvents(), [](const auto& event)
        { return event.LayerName == "First/Action"; }), 2);
    EXPECT_EQ(std::ranges::count_if(animation->GetNotifyEvents(), [](const auto& event)
        { return event.LayerName == "Second/Action"; }), 2);
}

TEST(AnimationGraphCompositionTest, MutedGraphInputsRestartWindowsWithNewActivationIds)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto clip = Clip(mesh, 0)->WithNotifies({{"Window", 0, 1}});
    const auto graph = AnimationGraph::Create({.Name = "Events", .Output = MakeNode(clip)});
    const auto animation = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation->Play(MakeNode(Clip(mesh, 0))));
    ASSERT_TRUE(animation->AddLayer("Overlay", Mask(mesh)));
    auto* layer = animation->GetLayer("Overlay");
    ASSERT_TRUE(layer->Play(MakeNode(graph->GetOutput())));
    ASSERT_TRUE(animation->Update(0.1f));
    ASSERT_EQ(animation->GetNotifyEvents().size(), 2);
    const auto firstId = animation->GetNotifyEvents()[0].ActivationId;
    ASSERT_TRUE(layer->SetWeight(0));
    ASSERT_TRUE(animation->Update(0.1f));
    ASSERT_EQ(animation->GetNotifyEvents().size(), 1);
    EXPECT_EQ(animation->GetNotifyEvents()[0].Type, EAnimationNotifyEventType::End);
    ASSERT_TRUE(layer->SetWeight(1));
    ASSERT_TRUE(animation->Update(0.1f));
    ASSERT_EQ(animation->GetNotifyEvents().size(), 2);
    EXPECT_EQ(animation->GetNotifyEvents()[0].Type, EAnimationNotifyEventType::Begin);
    EXPECT_NE(animation->GetNotifyEvents()[0].ActivationId, firstId);
}

TEST(AnimationGraphCompositionTest, MontageExitRestoresAllGraphWindowsWithinTheSameFrame)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto clip = Clip(mesh, 0)->WithNotifies({{"Window", 0, 1}});
    const auto blend = BlendNode::Create("Composition", {
        {.Name = "Locomotion", .Node = MakeNode(clip)},
        {.Name = "Action", .Mask = Mask(mesh), .Node = MakeNode(clip)}
    });
    const auto graph = AnimationGraph::Create({.Name = "Events", .Output = MakeNode(blend)});
    const auto animation = graph->CreateAnimation(mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    ASSERT_TRUE(animation->Update(0.1f));
    std::unordered_map<std::string, uint64_t> previousIds;
    for (const auto& event : animation->GetNotifyEvents())
    {
        if (event.Type == EAnimationNotifyEventType::Begin)
            ASSERT_TRUE(previousIds.emplace(event.LayerName, event.ActivationId).second);
    }
    ASSERT_EQ(previousIds.size(), 2);
    const auto action = AnimationSequence::Create("Action", mesh->GetSkeleton(), 0.2f, {});
    const auto montage = AnimationMontage::Create({.Name = "Action", .Sequence = action,
        .BlendIn = 0, .BlendOut = 0});
    ASSERT_TRUE(animation->GetBaseLayer().PlayMontage(montage));
    ASSERT_TRUE(animation->Update(0.3f));
    const auto events = animation->GetNotifyEvents();
    EXPECT_EQ(std::ranges::count_if(events, [](const auto& event)
        { return event.Type == EAnimationNotifyEventType::End && event.Cancelled; }), 2);
    EXPECT_EQ(std::ranges::count_if(events, [](const auto& event)
        { return event.Type == EAnimationNotifyEventType::Begin; }), 2);
    EXPECT_EQ(std::ranges::count_if(events, [](const auto& event)
        { return event.Type == EAnimationNotifyEventType::Update; }), 2);
    std::unordered_map<std::string, uint64_t> resumedIds;
    for (const auto& event : events)
    {
        EXPECT_EQ(event.Sequence, clip);
        EXPECT_EQ(event.NotifyIndex, 0);
        if (event.Type == EAnimationNotifyEventType::End && event.Cancelled)
        {
            const auto previous = previousIds.find(event.LayerName);
            ASSERT_NE(previous, previousIds.end());
            EXPECT_EQ(event.ActivationId, previous->second);
            EXPECT_DOUBLE_EQ(event.TimeOffset, 0);
        }
        if (event.Type == EAnimationNotifyEventType::Begin)
        {
            EXPECT_NEAR(event.TimeOffset, 0.2, 1e-6);
            EXPECT_NE(event.ActivationId, 0);
            ASSERT_TRUE(previousIds.contains(event.LayerName));
            // A resumed child activation may keep its ID when the montage only hid its events.
            ASSERT_TRUE(resumedIds.emplace(event.LayerName, event.ActivationId).second);
        }
        if (event.Type == EAnimationNotifyEventType::Update)
            EXPECT_NEAR(event.DeltaSeconds, 0.1, 1e-6);
    }
    ASSERT_EQ(resumedIds.size(), 2);
    for (const auto& event : events)
    {
        if (event.Type != EAnimationNotifyEventType::Update) continue;
        const auto resumed = resumedIds.find(event.LayerName);
        ASSERT_NE(resumed, resumedIds.end());
        EXPECT_EQ(event.ActivationId, resumed->second);
    }
    ASSERT_TRUE(animation->Update(0.1f));
    ASSERT_EQ(animation->GetNotifyEvents().size(), 2);
    for (const auto& event : animation->GetNotifyEvents())
    {
        EXPECT_EQ(event.Type, EAnimationNotifyEventType::Update);
        const auto resumed = resumedIds.find(event.LayerName);
        ASSERT_NE(resumed, resumedIds.end());
        EXPECT_EQ(event.ActivationId, resumed->second);
    }
}
