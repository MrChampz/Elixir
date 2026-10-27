#include "AnimationTestNodes.h"
#include "AnimationTestCharacter.h"
#include <gtest/gtest.h>

#include "AnimationGraphTestDriver.h"
#include "../Mesh/SkinningTestData.h"

#include <limits>

using namespace Elixir;

namespace
{
    class AnimationGraphTest : public testing::Test
    {
    protected:
        /** @brief Create a small rig and three clips with distinct root positions. */
        void SetUp() override
        {
            Mesh = SkinningTests::MakeMesh();
            ASSERT_TRUE(Mesh);
            const auto skeleton = Mesh->GetSkeleton();
            Idle = Sequence::Create("Idle", skeleton, 2, {
                { .BoneIndex = 0, .Translations = {{ 0, { 0, 0, 0 } }} },
            });
            Walk = Sequence::Create("Walk", skeleton, 2, {
                { .BoneIndex = 0, .Translations = {{ 0, { 4, 0, 0 } }} },
            });
            Run = Sequence::Create("Run", skeleton, 2, {
                { .BoneIndex = 0, .Translations = {{ 0, { 8, 0, 0 } }} },
            });
            ASSERT_TRUE(Idle);
            ASSERT_TRUE(Walk);
            ASSERT_TRUE(Run);
        }

        /** @brief Define locomotion with an extra boolean condition for running. */
        SAnimationGraphTestInfo MakeInfo() const
        {
            return {
                .Name = "Locomotion",
                .Parameters = {{ "Speed", 0.0f }, { "Grounded", true }},
                .Layers = {{ .Node = MakeNode(StateMachine::Create({
                    .InitialState = "Idle",
                    .States = {{ "Idle", MakeNode(Idle) }, { "Walk", MakeNode(Walk) }, { "Run", MakeNode(Run) }},
                    .Transitions = {
                    { .From = "Idle", .To = "Run",
                      .Conditions = {{ "Speed", EAnimationComparison::GreaterEqual, 3.0f },
                          { "Grounded", EAnimationComparison::Equal, true }},
                      .FadeDuration = 1.0f },
                    { .From = "Idle", .To = "Walk",
                      .Conditions = {{ "Speed", EAnimationComparison::Greater, 0.1f },
                          { "Speed", EAnimationComparison::Less, 3.0f }},
                      .FadeDuration = 1.0f },
                    { .From = "Walk", .To = "Run",
                      .Conditions = {{ "Speed", EAnimationComparison::GreaterEqual, 3.0f },
                          { "Grounded", EAnimationComparison::Equal, true }},
                      .FadeDuration = 1.0f },
                    { .From = "Run", .To = "Idle",
                      .Conditions = {{ "Speed", EAnimationComparison::LessEqual, 0.1f }},
                      .FadeDuration = 1.0f },
                }
                })) }},
            };
        }

        Ref<SkeletalMesh> Mesh;
        Ref<Sequence> Idle;
        Ref<Sequence> Walk;
        Ref<Sequence> Run;
    };
}

TEST_F(AnimationGraphTest, StartsTheInitialStateAndDoesNotRestartItEachFrame)
{
    const auto controller = CreateTestGraph(MakeInfo());
    ASSERT_TRUE(controller);
    EXPECT_EQ(controller->GetName(), "Locomotion");
    EXPECT_EQ(controller->GetSkeleton(), Mesh->GetSkeleton());
    EXPECT_EQ(std::static_pointer_cast<const StateMachineNode>(GetTestGraphInputs(*controller)[0].Node)->GetStateMachine()->GetCreateInfo().States.size(), 3);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.25f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.75f);
    EXPECT_FALSE(mesh.IsTransitioning());
}

TEST_F(AnimationGraphTest, CreatesAComposedAnimationWithoutEvaluatingStateTransitions)
{
    auto info = MakeInfo();
    EditMachine(info.Layers[0].Node, [](auto& machine)
    {
        machine.Transitions = {{.From = "Idle", .To = "Walk", .FadeDuration = 0}};
    });
    info.Layers.push_back({
        .Name = "Overlay",
        .Mask = BoneMask::Create("Half", Mesh->GetSkeleton(), {0.5f, 0.5f}),
        .Node = MakeNode(Walk)
    });
    const auto controller = CreateTestGraph(std::move(info));
    ASSERT_TRUE(controller);
    const auto animation = controller->CreateAnimation(Mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    EXPECT_EQ(animation->GetLayerCount(), 2);
    EXPECT_EQ(animation->GetCurrentState(), "Idle");
    EXPECT_FLOAT_EQ(animation->GetLocalPose().LocalTransforms[0].Translation.x, 2);
    EXPECT_FLOAT_EQ(animation->GetTime(), 0);
    EXPECT_TRUE(animation->GetNotifyEvents().empty());
    ASSERT_TRUE(animation->Update(0));
    EXPECT_EQ(animation->GetCurrentState(), "Walk");
    EXPECT_FLOAT_EQ(animation->GetLocalPose().LocalTransforms[0].Translation.x, 4);
}

TEST_F(AnimationGraphTest, GeneratedAnimationsOutliveTheirGraph)
{
    auto controller = CreateTestGraph(MakeInfo());
    const std::weak_ptr<const AnimationGraph> weakController = controller;
    const auto animation = controller->CreateAnimation(Mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    controller.reset();
    EXPECT_TRUE(weakController.expired());
    ASSERT_TRUE(animation->GetParameters().SetFloat("Speed", 4));
    ASSERT_TRUE(animation->Update(1));
    EXPECT_EQ(animation->GetCurrentState(), "Run");
    EXPECT_FLOAT_EQ(animation->GetLocalPose().LocalTransforms[0].Translation.x, 8);
}

TEST_F(AnimationGraphTest, RejectsInvalidReferencePosesBeforePublishingAnAnimation)
{
    const auto controller = CreateTestGraph(MakeInfo());
    EXPECT_FALSE(controller->CreateAnimation({}));
    EXPECT_FALSE(Animation::Create(nullptr, Mesh->GetData().ReferencePose));
    auto reference = Mesh->GetData().ReferencePose;
    reference.LocalTransforms[0].Translation.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(controller->CreateAnimation(reference));
}

TEST_F(AnimationGraphTest, RejectsIncompatibleAnimationsWithoutReplacingTheCurrentOne)
{
    const auto mesh = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(mesh);
    const auto* previous = &mesh->GetAnimation();
    EXPECT_FALSE(mesh->ReplaceAnimation(nullptr));
    const auto skeleton = Skeleton::Create("Different", {{.Name = "Different"}});
    const SLocalPose reference{{SBoneTransform{}}};
    const auto incompatible = Animation::Create(skeleton, reference);
    ASSERT_TRUE(incompatible);
    EXPECT_FALSE(mesh->ReplaceAnimation(incompatible));
    EXPECT_EQ(&mesh->GetAnimation(), previous);
    EXPECT_TRUE(mesh->Update(0));
}

TEST_F(AnimationGraphTest, ConditionsDriveCrossFadesAndAllowContinuousInterruption)
{
    const auto controller = CreateTestGraph(MakeInfo());
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.0f));
    ASSERT_TRUE(instance.SetFloat("Speed", 1.5f));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
    EXPECT_NEAR(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 2.0f, 1e-5f);
    ASSERT_TRUE(instance.SetFloat("Speed", 4.0f));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Run");
    EXPECT_NEAR(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 2.0f, 1e-5f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_NEAR(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 5.0f, 1e-5f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FALSE(mesh.IsTransitioning());
    EXPECT_NEAR(mesh.GetModelPose().Matrices[0][3].x, 8.0f, 1e-5f);
    ASSERT_TRUE(instance.SetFloat("Speed", 0.0f));
    ASSERT_TRUE(instance.Update(1.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    EXPECT_NEAR(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 0.0f, 1e-5f);
}

TEST_F(AnimationGraphTest, RequiresEveryNumericAndBooleanCondition)
{
    const auto controller = CreateTestGraph(MakeInfo());
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.SetFloat("Speed", 4.0f));
    ASSERT_TRUE(instance.SetBool("Grounded", false));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    ASSERT_TRUE(instance.SetBool("Grounded", true));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Run");
}

TEST_F(AnimationGraphTest, ResolvesPriorityAndTakesAtMostOneTransitionPerUpdate)
{
    auto info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions = {
        { .From = "Idle", .To = "Walk", .FadeDuration = 0.0f },
        { .From = "Idle", .To = "Run", .FadeDuration = 0.0f },
        { .From = "Walk", .To = "Run", .FadeDuration = 0.0f },
        { .From = "Run", .To = "Idle", .FadeDuration = 0.0f },
    };
    });
    const auto controller = CreateTestGraph(std::move(info));
    ASSERT_TRUE(controller);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 4.0f);
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Run");
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
}

TEST_F(AnimationGraphTest, CompletionDoesNotTreatPauseAsTheEndOfAnAnimation)
{
    auto info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.InitialState = "Walk";
        machine.States[1].Looping = false;
        machine.Transitions = {{ .From = "Walk", .To = "Idle", .WaitForCompletion = true }};
    });
    const auto controller = CreateTestGraph(std::move(info));
    ASSERT_TRUE(controller);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.5f));
    mesh.Pause();
    ASSERT_TRUE(instance.Update(10.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    mesh.Resume();
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    EXPECT_TRUE(mesh.IsTransitioning());
}

TEST_F(AnimationGraphTest, CompletionWaitsForAnIncomingFadeEvenAfterTheClipEnds)
{
    auto info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.States[1].Looping = false;
        machine.Transitions = {
        { .From = "Idle", .To = "Walk", .FadeDuration = 4.0f },
        { .From = "Walk", .To = "Run", .WaitForCompletion = true },
    };
    });
    const auto controller = CreateTestGraph(std::move(info));
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.Update(2.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
    EXPECT_TRUE(mesh.IsTransitioning());
    ASSERT_TRUE(instance.Update(1.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
    ASSERT_TRUE(instance.Update(1.0f));
    EXPECT_FALSE(mesh.IsTransitioning());
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Run");
}

TEST_F(AnimationGraphTest, SharedDefinitionsKeepParametersAndPlaybackIndependent)
{
    const auto controller = CreateTestGraph(MakeInfo());
    const auto firstMeshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(firstMeshHandle);
    auto& firstMesh = *firstMeshHandle;
    const auto secondMeshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(secondMeshHandle);
    auto& secondMesh = *secondMeshHandle;
    AnimationGraphTestDriver first(controller, firstMesh), second(controller, secondMesh);
    ASSERT_TRUE(first.SetFloat("Speed", 1.5f));
    ASSERT_TRUE(second.SetFloat("Speed", 4.0f));
    ASSERT_TRUE(first.Update(0.5f));
    ASSERT_TRUE(second.Update(1.0f));
    EXPECT_EQ(first.GetCurrentState(), "Walk");
    EXPECT_EQ(second.GetCurrentState(), "Run");
    EXPECT_FLOAT_EQ(firstMesh.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(secondMesh.GetTime(), 1.0f);
    EXPECT_EQ(first.GetFloat("Speed"), 1.5f);
    EXPECT_EQ(second.GetFloat("Speed"), 4.0f);
    first.Reset();
    EXPECT_EQ(first.GetCurrentState(), "Idle");
    EXPECT_EQ(first.GetFloat("Speed"), 0.0f);
    ASSERT_TRUE(first.Update(0.0f));
    EXPECT_EQ(first.GetCurrentState(), "Idle");
    EXPECT_FLOAT_EQ(firstMesh.GetTime(), 0.0f);
    EXPECT_EQ(second.GetCurrentState(), "Run");
}

TEST_F(AnimationGraphTest, RejectsInvalidParameterUpdatesWithoutChangingValues)
{
    const auto controller = CreateTestGraph(MakeInfo());
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    EXPECT_FALSE(instance.SetFloat("Missing", 1.0f));
    EXPECT_FALSE(instance.SetBool("Missing", true));
    EXPECT_FALSE(instance.SetBool("Speed", true));
    EXPECT_FALSE(instance.SetFloat("Grounded", 1.0f));
    EXPECT_FALSE(instance.SetFloat("Speed", std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(instance.SetFloat("Speed", std::numeric_limits<float>::quiet_NaN()));
    EXPECT_EQ(instance.GetFloat("Speed"), 0.0f);
    EXPECT_EQ(instance.GetBool("Grounded"), true);
    EXPECT_FALSE(instance.GetBool("Speed"));
    EXPECT_FALSE(instance.GetFloat("Grounded"));
    EXPECT_FALSE(instance.GetFloat("Missing"));
    EXPECT_FALSE(instance.GetBool("Missing"));
}

TEST_F(AnimationGraphTest, InvalidUpdatesPreserveTheCurrentPlayback)
{
    const auto controller = CreateTestGraph(MakeInfo());
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    EXPECT_FALSE(instance.Update(-1.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FALSE(instance.Update(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(instance.Update(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    EXPECT_FALSE(AnimationTestCharacter::Create(nullptr));
    AnimationGraphTestDriver missing(nullptr, mesh);
    EXPECT_FALSE(missing.Update(0.0f));
    EXPECT_FALSE(missing.SetFloat("Speed", 1.0f));
}

TEST_F(AnimationGraphTest, AcceptsCompatibleSkeletonsAndBindsEachMeshToANewInstance)
{
    const auto controller = CreateTestGraph(MakeInfo());
    auto other = SkinningTests::MakeMesh();
    ASSERT_NE(other->GetSkeleton(), Mesh->GetSkeleton());
    const auto meshHandle = AnimationTestCharacter::Create(other);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.5f));
    const auto replacementHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(replacementHandle);
    auto& replacement = *replacementHandle;
    AnimationGraphTestDriver replacementController(controller, replacement);
    ASSERT_TRUE(replacementController.Update(0.25f));
    EXPECT_FLOAT_EQ(replacement.GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    auto data = Mesh->GetData();
    data.Skeleton = Skeleton::Create("Other", {{ .Name = "Other" }, { .Name = "Child" }});
    auto incompatible = SkeletalMesh::Create(std::move(data));
    ASSERT_TRUE(incompatible);
    const auto invalidMeshHandle = AnimationTestCharacter::Create(incompatible);
    ASSERT_TRUE(invalidMeshHandle);
    auto& invalidMesh = *invalidMeshHandle;
    AnimationGraphTestDriver invalid(controller, invalidMesh);
    EXPECT_FALSE(invalid.Update(0.5f));
    invalid.Reset();
    EXPECT_FALSE(invalid.Update(0.5f));
    EXPECT_FLOAT_EQ(invalidMesh.GetTime(), 0.0f);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.75f);
}

TEST_F(AnimationGraphTest, RejectsInvalidStatesParametersAndInitialState)
{
    auto info = MakeInfo();
    info.Name.clear();
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.InitialState = "Missing";
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.States.clear();
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.States[1].Name = "Idle";
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.States[1].Node.reset();
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.States[1].Node = MakeNode(Sequence::Create("Other",
        Skeleton::Create("Other", {{ .Name = "Other" }}), 0, {}));
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    info.Parameters.push_back({ "Speed", 1.0f });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    info.Parameters[0].DefaultValue = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(CreateTestGraph(info));
}

TEST_F(AnimationGraphTest, RejectsInvalidTransitionEndpointsDurationsAndConditions)
{
    auto info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions[0].To = "Missing";
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions[0].To = "Idle";
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions[0].From = "Missing";
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions[0].FadeDuration = -1.0f;
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions[0].FadeDuration = std::numeric_limits<float>::quiet_NaN();
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions[0].WaitForCompletion = true;
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions[0].Conditions[0].Parameter = "Missing";
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions[0].Conditions[0].Value = true;
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions[0].Conditions[1].Comparison = EAnimationComparison::Greater;
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions[0].Conditions[0].Comparison = EAnimationComparison(255);
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions[0].Conditions[0].Value = std::numeric_limits<float>::infinity();
    });
    EXPECT_FALSE(CreateTestGraph(info));
}

TEST_F(AnimationGraphTest, SupportsAllNumericComparisonOperatorsAndBooleanInequality)
{
    const std::array comparisons{
        EAnimationComparison::Equal, EAnimationComparison::NotEqual,
        EAnimationComparison::Less, EAnimationComparison::LessEqual,
        EAnimationComparison::Greater, EAnimationComparison::GreaterEqual,
    };
    for (const auto comparison : comparisons)
    {
        for (const float speed : { 0.0f, 1.0f, 2.0f })
        {
            SCOPED_TRACE(testing::Message() << int(comparison) << ": " << speed);
            auto info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions = {{ .From = "Idle", .To = "Walk",
                .Conditions = {{ "Speed", comparison, 1.0f }}, .FadeDuration = 0.0f }};
    });
            const auto controller = CreateTestGraph(std::move(info));
            ASSERT_TRUE(controller);
            const auto meshHandle = AnimationTestCharacter::Create(Mesh);
            ASSERT_TRUE(meshHandle);
            auto& mesh = *meshHandle;
            AnimationGraphTestDriver instance(controller, mesh);
            ASSERT_TRUE(instance.SetFloat("Speed", speed));
            ASSERT_TRUE(instance.Update(0.0f));
            bool expected = false;
            switch (comparison)
            {
                case EAnimationComparison::Equal: expected = speed == 1.0f; break;
                case EAnimationComparison::NotEqual: expected = speed != 1.0f; break;
                case EAnimationComparison::Less: expected = speed < 1.0f; break;
                case EAnimationComparison::LessEqual: expected = speed <= 1.0f; break;
                case EAnimationComparison::Greater: expected = speed > 1.0f; break;
                case EAnimationComparison::GreaterEqual: expected = speed >= 1.0f; break;
            }
            EXPECT_EQ(instance.GetCurrentState(), expected ? "Walk" : "Idle");
        }
    }
    auto info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions = {{ .From = "Idle", .To = "Walk",
        .Conditions = {{ "Grounded", EAnimationComparison::NotEqual, true }} }};
    });
    const auto controller = CreateTestGraph(std::move(info));
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    ASSERT_TRUE(instance.SetBool("Grounded", false));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
}

TEST_F(AnimationGraphTest, FailedPoseUpdatesRetainTheCommittedStateAndLastValidProgress)
{
    auto info = MakeInfo();
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.States[1].Node = MakeNode(Sequence::Create("Mirror", Mesh->GetSkeleton(), 2, {
        { .BoneIndex = 0, .Scales = {{ 0, { -1, 1, 1 } }} },
    }));
    });
    const auto controller = CreateTestGraph(std::move(info));
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.0f));
    ASSERT_TRUE(instance.SetFloat("Speed", 1.5f));
    EXPECT_FALSE(instance.Update(0.5f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.0f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Scale.x, 1.0f);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Scale.x, 0.5f);
}

TEST_F(AnimationGraphTest, OneGraphDrivesIndependentMachinesAndAdvancesEveryLayerOnce)
{
    auto info = MakeInfo();
    const auto action = Sequence::Create("Action", Mesh->GetSkeleton(), 2, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 3, 0 } }} },
    });
    info.Parameters.push_back({ "Attack", false });
    info.Layers.push_back({
        .Name = "UpperBody", .Mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child"),
        .Node = MakeNode(StateMachine::Create({
            .InitialState = "Idle",
            .States = {{ "Idle", MakeNode(Idle) }, { .Name = "Attack", .Node = MakeNode(action),
                .Looping = false }},
            .Transitions = {
                { .From = "Idle", .To = "Attack",
                  .Conditions = {{ "Attack", EAnimationComparison::Equal, true }},
                  .FadeDuration = 0 },
                { .From = "Attack", .To = "Idle", .FadeDuration = 0,
                  .WaitForCompletion = true },
            },
        })),
    });
    const auto controller = CreateTestGraph(info);
    ASSERT_TRUE(controller);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    EXPECT_EQ(instance.GetCurrentState("UpperBody"), "Idle");
    EXPECT_EQ(mesh.GetLayerCount(), 2);
    ASSERT_TRUE(instance.SetFloat("Speed", 1.5f));
    ASSERT_TRUE(instance.SetBool("Attack", true));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_EQ(mesh.GetLayerCount(), 2);
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
    EXPECT_EQ(instance.GetCurrentState("UpperBody"), "Attack");
    EXPECT_TRUE(instance.GetCurrentState("Missing").empty());
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("UpperBody")->GetPlayer().GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 2);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[1].Translation.y, 3);
    ASSERT_TRUE(instance.SetBool("Attack", false));
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_EQ(instance.GetCurrentState("UpperBody"), "Attack");
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetCurrentState("UpperBody"), "Idle");
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0);
}

TEST_F(AnimationGraphTest, SharedParametersDriveAllMachinesAndResetRestartsThemTogether)
{
    auto info = MakeInfo();
    auto upper = info.Layers.front();
    upper.Name = "UpperBody";
    upper.Mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child");
    upper.Weight = 0.5f;
    info.Layers.push_back(upper);
    const auto controller = CreateTestGraph(info);
    ASSERT_TRUE(controller);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    const auto otherHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(otherHandle);
    auto& other = *otherHandle;
    AnimationGraphTestDriver instance(controller, mesh), second(controller, other);
    ASSERT_TRUE(instance.SetFloat("Speed", 4));
    ASSERT_TRUE(instance.Update(0.75f));
    ASSERT_TRUE(second.Update(0.25f));
    EXPECT_EQ(instance.GetCurrentState(), "Run");
    EXPECT_EQ(instance.GetCurrentState("UpperBody"), "Run");
    EXPECT_EQ(second.GetCurrentState(), "Idle");
    EXPECT_EQ(second.GetCurrentState("UpperBody"), "Idle");
    EXPECT_FLOAT_EQ(other.GetTime(), 0.25f);
    instance.Reset();
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    EXPECT_EQ(instance.GetCurrentState("UpperBody"), "Idle");
    EXPECT_EQ(instance.GetFloat("Speed"), 0);
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    EXPECT_EQ(instance.GetCurrentState("UpperBody"), "Idle");
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.1f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("UpperBody")->GetPlayer().GetTime(), 0.1f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("UpperBody")->GetWeight(), 0.5f);
    EXPECT_FLOAT_EQ(other.GetTime(), 0.25f);
}

TEST_F(AnimationGraphTest, GraphConfiguresOrderedOverrideAndAdditiveLayersWithOwnedEvents)
{
    auto info = MakeInfo();
    const auto mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child");
    const auto makeClip = [&](const char* name, const float height)
    {
        return Sequence::Create(name, Mesh->GetSkeleton(), 2, {
            { .BoneIndex = 1, .Translations = {{ 0, { 0, height, 0 } }} },
        }, {}, {{ "Event", 0.25f }});
    };
    const auto action = makeClip("Action", 3);
    const auto recoil = makeClip("Recoil", 2);
    auto reference = Mesh->GetData().ReferencePose;
    reference.LocalTransforms[1].Translation.y = 1;
    info.Layers.push_back({ .Name = "Action", .Mask = mask,
        .Node = MakeNode(StateMachine::Create({ .InitialState = "Active", .States = {{ "Active", MakeNode(action) }} })) });
    info.Layers.push_back({ .Name = "Recoil", .Mask = mask, .Weight = 0.5f,
        .BlendMode = EAnimationLayerBlendMode::Additive, .AdditiveReferencePose = reference,
        .Node = MakeNode(StateMachine::Create({ .InitialState = "Active", .States = {{ "Active", MakeNode(recoil) }} })) });
    const auto controller = CreateTestGraph(info);
    ASSERT_TRUE(controller);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_EQ(mesh.GetLayerCount(), 3);
    EXPECT_EQ(mesh.GetLayer("Recoil")->GetBlendMode(), EAnimationLayerBlendMode::Additive);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[1].Translation.y, 3.5f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("Action")->GetPlayer().GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("Recoil")->GetPlayer().GetTime(), 0.5f);
    ASSERT_EQ(mesh.GetNotifyEvents().size(), 2);
    EXPECT_EQ(mesh.GetNotifyEvents()[0].LayerName, "Action");
    EXPECT_EQ(mesh.GetNotifyEvents()[1].LayerName, "Recoil");
    EXPECT_DOUBLE_EQ(mesh.GetNotifyEvents()[0].TimeOffset, 0.25);
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_TRUE(mesh.GetNotifyEvents().empty());
    ASSERT_TRUE(mesh.GetLayer("Recoil")->SetWeight(0));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[1].Translation.y, 3);
}

TEST_F(AnimationGraphTest, GraphConnectionsStayFixedAndCanBeMutedAtRuntime)
{
    auto info = MakeInfo();
    info.Layers.push_back({ .Name = "Action",
        .Mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child"),
        .Node = MakeNode(info.Layers.front().Node) });
    const auto controller = CreateTestGraph(info);
    ASSERT_TRUE(controller);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    EXPECT_FALSE(instance.Update(-1));
    EXPECT_EQ(mesh.GetLayerCount(), 2);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FALSE(mesh.RemoveLayer("Action"));
    ASSERT_TRUE(mesh.GetLayer("Action")->SetWeight(0));
    ASSERT_TRUE(instance.SetFloat("Speed", 4));
    ASSERT_TRUE(instance.Update(1));
    EXPECT_EQ(instance.GetCurrentState(), "Run");
    EXPECT_EQ(instance.GetCurrentState("Action"), "Run");
    EXPECT_FLOAT_EQ(mesh.GetTime(), 1);
}

TEST_F(AnimationGraphTest, CreatingAnAnimationDoesNotReplaceExistingMeshPlayback)
{
    const auto controller = CreateTestGraph(MakeInfo());
    const auto mesh = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(mesh);
    const auto mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child");
    ASSERT_TRUE(mesh->AddLayer("Existing", mask));
    ASSERT_TRUE(mesh->GetLayer("Existing")->GetPlayer().Play(MakeNode(Walk)));
    ASSERT_TRUE(mesh->Update(0.25f));

    auto animation = controller->CreateAnimation(Mesh->GetData().ReferencePose);
    ASSERT_TRUE(animation);
    EXPECT_EQ(mesh->GetLayerCount(), 2);
    EXPECT_FLOAT_EQ(mesh->GetLayer("Existing")->GetPlayer().GetTime(), 0.25f);
    EXPECT_EQ(animation->GetLayerCount(), 1);
    EXPECT_EQ(animation->GetCurrentState(), "Idle");
    EXPECT_TRUE(mesh->ReplaceAnimation(std::move(animation)));
    EXPECT_EQ(mesh->GetLayerCount(), 1);
}

TEST_F(AnimationGraphTest, RejectsInvalidLayerDefinitionsAndCrossMachineSkeletons)
{
    auto valid = MakeInfo();
    valid.Layers.push_back({ .Name = "Action",
        .Mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child"),
        .Node = MakeNode(valid.Layers.front().Node) });
    ASSERT_TRUE(CreateTestGraph(valid));
    auto info = valid;
    info.Layers.clear();
    EXPECT_FALSE(CreateTestGraph(info));
    info = valid;
    info.Layers[0].Name = "Other";
    EXPECT_TRUE(CreateTestGraph(info));
    info = valid;
    info.Layers[1].Name = "Base";
    EXPECT_FALSE(CreateTestGraph(info));
    info = valid;
    info.Layers.push_back(info.Layers.back());
    EXPECT_FALSE(CreateTestGraph(info));
    info = valid;
    info.Layers[1].Mask.reset();
    EXPECT_FALSE(CreateTestGraph(info));
    info = valid;
    info.Layers[0].Weight = 0.5f;
    EXPECT_FALSE(CreateTestGraph(info));
    info = valid;
    info.Layers[0].BlendMode = EAnimationLayerBlendMode::Additive;
    EXPECT_FALSE(CreateTestGraph(info));
    info = valid;
    info.Layers[1].Weight = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(CreateTestGraph(info));
    info = valid;
    info.Layers[1].AdditiveReferencePose.LocalTransforms.resize(1);
    EXPECT_FALSE(CreateTestGraph(info));
    info = valid;
    info.Layers[1].AdditiveReferencePose = Mesh->GetData().ReferencePose;
    info.Layers[1].AdditiveReferencePose.LocalTransforms[0].Scale.x = 0;
    EXPECT_FALSE(CreateTestGraph(info));
    info = valid;
    EditMachine(info.Layers[1].Node, [&](auto& machine)
    {
        machine.InitialState = "Missing";
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = valid;
    const auto other = Skeleton::Create("Other", {{ .Name = "Other" }});
    EditMachine(info.Layers[1].Node, [&](auto& machine)
    {
        machine.States = {{ "Other",
        MakeNode(Sequence::Create("Other", other, 1, {})) }};
        machine.InitialState = "Other";
        machine.Transitions.clear();
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info = valid;
    info.Layers[1].Mask = BoneMask::FromBone("Other", other, "Other");
    EXPECT_FALSE(CreateTestGraph(info));
}

TEST_F(AnimationGraphTest, GlobalPauseAndIndividualLayerPauseKeepIndependentClocks)
{
    auto info = MakeInfo();
    info.Layers.push_back({ .Name = "Action",
        .Mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child"),
        .Node = MakeNode(info.Layers.front().Node) });
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(CreateTestGraph(info), mesh);
    ASSERT_TRUE(instance.Update(0.25f));
    auto& action = *mesh.GetLayer("Action");
    action.GetPlayer().Pause();
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(action.GetPlayer().GetTime(), 0.25f);
    mesh.Pause();
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(action.GetPlayer().GetTime(), 0.25f);
    mesh.Resume();
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.75f);
    EXPECT_FLOAT_EQ(action.GetPlayer().GetTime(), 0.25f);
    action.GetPlayer().Resume();
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 1);
    EXPECT_FLOAT_EQ(action.GetPlayer().GetTime(), 0.5f);
}

TEST_F(AnimationGraphTest, FailedCompositionKeepsEveryLayerClockAndLastValidPose)
{
    auto info = MakeInfo();
    const auto mirror = Sequence::Create("Mirror", Mesh->GetSkeleton(), 2, {
        { .BoneIndex = 1, .Scales = {{ 0, { -1, 1, 1 } }} },
    });
    info.Parameters.push_back({ "Attack", false });
    info.Layers.push_back({ .Name = "Action",
        .Mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child"),
        .Node = MakeNode(StateMachine::Create({
            .InitialState = "Idle", .States = {{ "Idle", MakeNode(Idle) }, { "Mirror", MakeNode(mirror) }},
            .Transitions = {{ .From = "Idle", .To = "Mirror",
                .Conditions = {{ "Attack", EAnimationComparison::Equal, true }},
                .FadeDuration = 1 }},
        })) });
    const auto controller = CreateTestGraph(info);
    ASSERT_TRUE(controller);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(instance.SetBool("Attack", true));
    EXPECT_FALSE(instance.Update(0.5f));
    EXPECT_EQ(instance.GetCurrentState("Action"), "Idle");
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("Action")->GetPlayer().GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[1].Scale.x, 1);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("Action")->GetPlayer().GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[1].Scale.x, 0.5f);
}

TEST_F(AnimationGraphTest, DirectSequenceStartsOnceHoldsItsEndAndRestartsOnReset)
{
    const auto sequence = Walk->WithNotifies({{ "Start", 0 }, { "Step", 0.25f }});
    const auto controller = CreateTestGraph({
        .Name = "DirectSequence",
        .Layers = {{ .Node = MakeNode(sequence), .Looping = false }},
    });
    ASSERT_TRUE(controller);
    const auto node = std::dynamic_pointer_cast<const SequenceNode>(
        GetTestGraphInputs(*controller)[0].Node);
    ASSERT_TRUE(node);
    EXPECT_EQ(node->GetSequence(), sequence);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_TRUE(instance.GetCurrentState().empty());
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 4);
    ASSERT_EQ(mesh.GetNotifyEvents().size(), 2);
    EXPECT_EQ(mesh.GetNotifyEvents()[0].LayerName, "Base");
    ASSERT_TRUE(instance.Update(2));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 2);
    EXPECT_FALSE(mesh.IsPlaying());
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 2);
    EXPECT_TRUE(mesh.GetNotifyEvents().empty());
    instance.Reset();
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.1f);
    EXPECT_TRUE(mesh.IsPlaying());
    ASSERT_EQ(mesh.GetNotifyEvents().size(), 1);
    EXPECT_EQ(mesh.GetNotifyEvents()[0].GetName(), "Start");
}

TEST_F(AnimationGraphTest, DirectSequenceMatchesASingleStateMachineThroughLoopsAndPause)
{
    const auto direct = CreateTestGraph({
        .Name = "Direct", .Layers = {{ .Node = MakeNode(Walk) }},
    });
    const auto machine = CreateTestGraph({
        .Name = "Machine", .Layers = {{ .Node = MakeNode(StateMachine::Create({
            .InitialState = "Walk", .States = {{ "Walk", MakeNode(Walk) }},
        })) }},
    });
    ASSERT_TRUE(direct);
    ASSERT_TRUE(machine);
    const auto directMeshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(directMeshHandle);
    auto& directMesh = *directMeshHandle;
    const auto machineMeshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(machineMeshHandle);
    auto& machineMesh = *machineMeshHandle;
    AnimationGraphTestDriver first(direct, directMesh), second(machine, machineMesh);
    for (const auto dt : { 0.0f, 0.25f, 1.5f, 2.75f })
    {
        ASSERT_TRUE(first.Update(dt));
        ASSERT_TRUE(second.Update(dt));
        EXPECT_FLOAT_EQ(directMesh.GetTime(), machineMesh.GetTime());
        EXPECT_FLOAT_EQ(directMesh.GetLocalPose().LocalTransforms[0].Translation.x,
            machineMesh.GetLocalPose().LocalTransforms[0].Translation.x);
    }
    const auto time = directMesh.GetTime();
    directMesh.Pause();
    ASSERT_TRUE(first.Update(1));
    EXPECT_FLOAT_EQ(directMesh.GetTime(), time);
    directMesh.Resume();
    ASSERT_TRUE(first.Update(0.25f));
    EXPECT_FLOAT_EQ(directMesh.GetTime(), time + 0.25f);
}

TEST_F(AnimationGraphTest, DirectAndMachineSourcesShareParametersAndComposeInLayerOrder)
{
    auto info = MakeInfo();
    info.Layers[0].Node = MakeNode(StateMachine::Create({}));
    info.Layers[0].Node = MakeNode(Walk);
    const auto mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child");
    const auto action = Sequence::Create("Action", Mesh->GetSkeleton(), 2, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 3, 0 } }} },
    });
    const auto recoil = Sequence::Create("Recoil", Mesh->GetSkeleton(), 2, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 2, 0 } }} },
    }, {}, {{ "Recoil", 0.25f }});
    info.Layers.push_back({ .Name = "Action", .Mask = mask, .Node = MakeNode(StateMachine::Create({
        .InitialState = "Idle", .States = {{ "Idle", MakeNode(Idle) }, { "Attack", MakeNode(action) }},
        .Transitions = {{ .From = "Idle", .To = "Attack", .Conditions = {
            { "Speed", EAnimationComparison::Greater, 0.1f } }, .FadeDuration = 0 }},
    })) });
    info.Layers.push_back({ .Name = "Recoil", .Mask = mask, .Weight = 0.5f,
        .BlendMode = EAnimationLayerBlendMode::Additive, .Node = MakeNode(recoil) });
    const auto controller = CreateTestGraph(info);
    ASSERT_TRUE(controller);
    const auto meshHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(meshHandle);
    auto& mesh = *meshHandle;
    AnimationGraphTestDriver instance(controller, mesh);
    ASSERT_TRUE(instance.SetFloat("Speed", 1));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_TRUE(instance.GetCurrentState().empty());
    EXPECT_EQ(instance.GetCurrentState("Action"), "Attack");
    EXPECT_TRUE(instance.GetCurrentState("Recoil").empty());
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("Action")->GetPlayer().GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("Recoil")->GetPlayer().GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[0].Translation.x, 4);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[1].Translation.y, 4);
    ASSERT_EQ(mesh.GetNotifyEvents().size(), 1);
    EXPECT_EQ(mesh.GetNotifyEvents()[0].LayerName, "Recoil");
    ASSERT_TRUE(mesh.GetLayer("Recoil")->SetWeight(0));
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_FLOAT_EQ(mesh.GetLocalPose().LocalTransforms[1].Translation.y, 3);
    EXPECT_TRUE(mesh.GetNotifyEvents().empty());
}

TEST_F(AnimationGraphTest, DirectSourceValidationRejectsMissingAmbiguousAndMisplacedSettings)
{
    SAnimationGraphTestInfo info{ .Name = "Direct", .Layers = {{ .Node = MakeNode(Walk) }} };
    ASSERT_TRUE(CreateTestGraph(info));
    info.Layers[0].Node.reset();
    EXPECT_FALSE(CreateTestGraph(info));
    info.Layers[0].Node = MakeNode(Walk);
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.InitialState = "Missing";
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info.Layers[0].Node = MakeNode(MakeInfo().Layers[0].Node);
    EXPECT_FALSE(CreateTestGraph(info));
    info.Layers[0].Node = MakeNode(StateMachine::Create({}));
    EditMachine(info.Layers[0].Node, [&](auto& machine)
    {
        machine.Transitions.push_back({ .From = "A", .To = "B" });
    });
    EXPECT_FALSE(CreateTestGraph(info));
    info.Layers[0].Node = MakeNode(StateMachine::Create({}));
    info.Layers[0].InputParameters = { "Speed" };
    EXPECT_FALSE(CreateTestGraph(info));
    info = MakeInfo();
    info.Layers[0].InputParameters = { "Speed" };
    EXPECT_FALSE(CreateTestGraph(info));
    info.Layers[0].InputParameters.clear();
    info.Layers[0].Looping = false;
    EXPECT_FALSE(CreateTestGraph(info));
    info = { .Name = "Direct", .Layers = {{ .Node = MakeNode(Walk) }} };
    const auto other = Skeleton::Create("Other", {{ .Name = "Other" }});
    info.Layers.push_back({ .Name = "Other",
        .Mask = BoneMask::FromBone("Other", other, "Other"),
        .Node = MakeNode(Sequence::Create("Other", other, 1, {})) });
    EXPECT_FALSE(CreateTestGraph(info));
}
