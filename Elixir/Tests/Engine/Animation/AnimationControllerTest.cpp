#include <gtest/gtest.h>

#include <Engine/Animation/AnimationControllerInstance.h>
#include "../Mesh/SkinningTestData.h"

#include <limits>

using namespace Elixir;

namespace
{
    class AnimationControllerTest : public testing::Test
    {
    protected:
        /** @brief Create a small rig and three clips with distinct root positions. */
        void SetUp() override
        {
            Mesh = SkinningTests::MakeMesh();
            ASSERT_TRUE(Mesh);
            const auto skeleton = Mesh->GetSkeleton();
            Idle = AnimationSequence::Create("Idle", skeleton, 2, {
                { .BoneIndex = 0, .Translations = {{ 0, { 0, 0, 0 } }} },
            });
            Walk = AnimationSequence::Create("Walk", skeleton, 2, {
                { .BoneIndex = 0, .Translations = {{ 0, { 4, 0, 0 } }} },
            });
            Run = AnimationSequence::Create("Run", skeleton, 2, {
                { .BoneIndex = 0, .Translations = {{ 0, { 8, 0, 0 } }} },
            });
            ASSERT_TRUE(Idle);
            ASSERT_TRUE(Walk);
            ASSERT_TRUE(Run);
        }

        /** @brief Define locomotion with an extra boolean condition for running. */
        SAnimationControllerCreateInfo MakeInfo() const
        {
            return {
                .Name = "Locomotion",
                .Parameters = {{ "Speed", 0.0f }, { "Grounded", true }},
                .Layers = {{ .StateMachine = {
                    .InitialState = "Idle",
                    .States = {{ "Idle", Idle }, { "Walk", Walk }, { "Run", Run }},
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
                } }},
            };
        }

        Ref<SkeletalMesh> Mesh;
        Ref<AnimationSequence> Idle;
        Ref<AnimationSequence> Walk;
        Ref<AnimationSequence> Run;
    };
}

TEST_F(AnimationControllerTest, StartsTheInitialStateAndDoesNotRestartItEachFrame)
{
    const auto controller = AnimationController::Create(MakeInfo());
    ASSERT_TRUE(controller);
    EXPECT_EQ(controller->GetName(), "Locomotion");
    EXPECT_EQ(controller->GetSkeleton(), Mesh->GetSkeleton());
    EXPECT_EQ(controller->GetLayers()[0].StateMachine.States.size(), 3);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    EXPECT_TRUE(instance.GetCurrentState().empty());
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.25f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.75f);
    EXPECT_FALSE(mesh.IsTransitioning());
}

TEST_F(AnimationControllerTest, ConditionsDriveCrossFadesAndAllowContinuousInterruption)
{
    const auto controller = AnimationController::Create(MakeInfo());
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.0f));
    ASSERT_TRUE(instance.SetFloat("Speed", 1.5f));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
    EXPECT_NEAR(mesh.GetLocalPose()[0].Translation.x, 2.0f, 1e-5f);
    ASSERT_TRUE(instance.SetFloat("Speed", 4.0f));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Run");
    EXPECT_NEAR(mesh.GetLocalPose()[0].Translation.x, 2.0f, 1e-5f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_NEAR(mesh.GetLocalPose()[0].Translation.x, 5.0f, 1e-5f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FALSE(mesh.IsTransitioning());
    EXPECT_NEAR(mesh.GetModelPose()[0][3].x, 8.0f, 1e-5f);
    ASSERT_TRUE(instance.SetFloat("Speed", 0.0f));
    ASSERT_TRUE(instance.Update(1.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    EXPECT_NEAR(mesh.GetLocalPose()[0].Translation.x, 0.0f, 1e-5f);
}

TEST_F(AnimationControllerTest, RequiresEveryNumericAndBooleanCondition)
{
    const auto controller = AnimationController::Create(MakeInfo());
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.SetFloat("Speed", 4.0f));
    ASSERT_TRUE(instance.SetBool("Grounded", false));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    ASSERT_TRUE(instance.SetBool("Grounded", true));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Run");
}

TEST_F(AnimationControllerTest, ResolvesPriorityAndTakesAtMostOneTransitionPerUpdate)
{
    auto info = MakeInfo();
    info.Layers[0].StateMachine.Transitions = {
        { .From = "Idle", .To = "Walk", .FadeDuration = 0.0f },
        { .From = "Idle", .To = "Run", .FadeDuration = 0.0f },
        { .From = "Walk", .To = "Run", .FadeDuration = 0.0f },
        { .From = "Run", .To = "Idle", .FadeDuration = 0.0f },
    };
    const auto controller = AnimationController::Create(std::move(info));
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Translation.x, 4.0f);
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Run");
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
}

TEST_F(AnimationControllerTest, CompletionDoesNotTreatPauseAsTheEndOfAnAnimation)
{
    auto info = MakeInfo();
    info.Layers[0].StateMachine.InitialState = "Walk";
    info.Layers[0].StateMachine.States[1].Looping = false;
    info.Layers[0].StateMachine.Transitions = {{ .From = "Walk", .To = "Idle", .WaitForCompletion = true }};
    const auto controller = AnimationController::Create(std::move(info));
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
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

TEST_F(AnimationControllerTest, CompletionWaitsForAnIncomingFadeEvenAfterTheClipEnds)
{
    auto info = MakeInfo();
    info.Layers[0].StateMachine.States[1].Looping = false;
    info.Layers[0].StateMachine.Transitions = {
        { .From = "Idle", .To = "Walk", .FadeDuration = 4.0f },
        { .From = "Walk", .To = "Run", .WaitForCompletion = true },
    };
    const auto controller = AnimationController::Create(std::move(info));
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
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

TEST_F(AnimationControllerTest, SharedDefinitionsKeepParametersAndPlaybackIndependent)
{
    const auto controller = AnimationController::Create(MakeInfo());
    SkeletalMeshInstance firstMesh(Mesh), secondMesh(Mesh);
    AnimationControllerInstance first(controller, firstMesh), second(controller, secondMesh);
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
    EXPECT_TRUE(first.GetCurrentState().empty());
    EXPECT_EQ(first.GetFloat("Speed"), 0.0f);
    ASSERT_TRUE(first.Update(0.0f));
    EXPECT_EQ(first.GetCurrentState(), "Idle");
    EXPECT_FLOAT_EQ(firstMesh.GetTime(), 0.0f);
    EXPECT_EQ(second.GetCurrentState(), "Run");
}

TEST_F(AnimationControllerTest, RejectsInvalidParameterUpdatesWithoutChangingValues)
{
    const auto controller = AnimationController::Create(MakeInfo());
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
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

TEST_F(AnimationControllerTest, InvalidUpdatesPreserveTheCurrentPlayback)
{
    const auto controller = AnimationController::Create(MakeInfo());
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    EXPECT_FALSE(instance.Update(-1.0f));
    EXPECT_TRUE(instance.GetCurrentState().empty());
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FALSE(instance.Update(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(instance.Update(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    SkeletalMeshInstance invalidMesh(nullptr);
    AnimationControllerInstance invalid(controller, invalidMesh);
    EXPECT_FALSE(invalid.Update(0.5f));
    AnimationControllerInstance missing(nullptr, mesh);
    EXPECT_FALSE(missing.Update(0.0f));
    EXPECT_FALSE(missing.SetFloat("Speed", 1.0f));
}

TEST_F(AnimationControllerTest, AcceptsCompatibleSkeletonsAndBindsEachMeshToANewInstance)
{
    const auto controller = AnimationController::Create(MakeInfo());
    auto other = SkinningTests::MakeMesh();
    ASSERT_NE(other->GetSkeleton(), Mesh->GetSkeleton());
    SkeletalMeshInstance mesh(other);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.5f));
    SkeletalMeshInstance replacement(Mesh);
    AnimationControllerInstance replacementController(controller, replacement);
    ASSERT_TRUE(replacementController.Update(0.25f));
    EXPECT_FLOAT_EQ(replacement.GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    auto data = Mesh->GetData();
    data.Skeleton = Skeleton::Create("Other", {{ .Name = "Other" }, { .Name = "Child" }});
    auto incompatible = SkeletalMesh::Create(std::move(data));
    ASSERT_TRUE(incompatible);
    SkeletalMeshInstance invalidMesh(incompatible);
    AnimationControllerInstance invalid(controller, invalidMesh);
    EXPECT_FALSE(invalid.Update(0.5f));
    invalid.Reset();
    EXPECT_FALSE(invalid.Update(0.5f));
    EXPECT_FLOAT_EQ(invalidMesh.GetTime(), 0.0f);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.75f);
}

TEST_F(AnimationControllerTest, RejectsInvalidStatesParametersAndInitialState)
{
    auto info = MakeInfo();
    info.Name.clear();
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.InitialState = "Missing";
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.States.clear();
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.States[1].Name = "Idle";
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.States[1].Sequence.reset();
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.States[1].Sequence = AnimationSequence::Create("Other",
        Skeleton::Create("Other", {{ .Name = "Other" }}), 0, {});
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Parameters.push_back({ "Speed", 1.0f });
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Parameters[0].DefaultValue = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(AnimationController::Create(info));
}

TEST_F(AnimationControllerTest, RejectsInvalidTransitionEndpointsDurationsAndConditions)
{
    auto info = MakeInfo();
    info.Layers[0].StateMachine.Transitions[0].To = "Missing";
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.Transitions[0].To = "Idle";
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.Transitions[0].From = "Missing";
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.Transitions[0].FadeDuration = -1.0f;
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.Transitions[0].FadeDuration = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.Transitions[0].WaitForCompletion = true;
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.Transitions[0].Conditions[0].Parameter = "Missing";
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.Transitions[0].Conditions[0].Value = true;
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.Transitions[0].Conditions[1].Comparison = EAnimationComparison::Greater;
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.Transitions[0].Conditions[0].Comparison = EAnimationComparison(255);
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].StateMachine.Transitions[0].Conditions[0].Value = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(AnimationController::Create(info));
}

TEST_F(AnimationControllerTest, SupportsAllNumericComparisonOperatorsAndBooleanInequality)
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
            info.Layers[0].StateMachine.Transitions = {{ .From = "Idle", .To = "Walk",
                .Conditions = {{ "Speed", comparison, 1.0f }}, .FadeDuration = 0.0f }};
            const auto controller = AnimationController::Create(std::move(info));
            ASSERT_TRUE(controller);
            SkeletalMeshInstance mesh(Mesh);
            AnimationControllerInstance instance(controller, mesh);
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
    info.Layers[0].StateMachine.Transitions = {{ .From = "Idle", .To = "Walk",
        .Conditions = {{ "Grounded", EAnimationComparison::NotEqual, true }} }};
    const auto controller = AnimationController::Create(std::move(info));
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    ASSERT_TRUE(instance.SetBool("Grounded", false));
    ASSERT_TRUE(instance.Update(0.0f));
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
}

TEST_F(AnimationControllerTest, FailedPoseUpdatesRetainTheTargetStateAndLastValidProgress)
{
    auto info = MakeInfo();
    info.Layers[0].StateMachine.States[1].Sequence = AnimationSequence::Create("Mirror", Mesh->GetSkeleton(), 2, {
        { .BoneIndex = 0, .Scales = {{ 0, { -1, 1, 1 } }} },
    });
    const auto controller = AnimationController::Create(std::move(info));
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.0f));
    ASSERT_TRUE(instance.SetFloat("Speed", 1.5f));
    EXPECT_FALSE(instance.Update(0.5f));
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.0f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Scale.x, 1.0f);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Scale.x, 0.5f);
}

TEST_F(AnimationControllerTest, OneControllerDrivesIndependentMachinesAndAdvancesEveryLayerOnce)
{
    auto info = MakeInfo();
    const auto action = AnimationSequence::Create("Action", Mesh->GetSkeleton(), 2, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 3, 0 } }} },
    });
    info.Parameters.push_back({ "Attack", false });
    info.Layers.push_back({
        .Name = "UpperBody", .Mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child"),
        .StateMachine = {
            .InitialState = "Idle",
            .States = {{ "Idle", Idle }, { .Name = "Attack", .Sequence = action,
                .Looping = false }},
            .Transitions = {
                { .From = "Idle", .To = "Attack",
                  .Conditions = {{ "Attack", EAnimationComparison::Equal, true }},
                  .FadeDuration = 0 },
                { .From = "Attack", .To = "Idle", .FadeDuration = 0,
                  .WaitForCompletion = true },
            },
        },
    });
    const auto controller = AnimationController::Create(info);
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    EXPECT_TRUE(instance.GetCurrentState("UpperBody").empty());
    EXPECT_EQ(mesh.GetLayerCount(), 1);
    ASSERT_TRUE(instance.SetFloat("Speed", 1.5f));
    ASSERT_TRUE(instance.SetBool("Attack", true));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_EQ(mesh.GetLayerCount(), 2);
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
    EXPECT_EQ(instance.GetCurrentState("UpperBody"), "Attack");
    EXPECT_TRUE(instance.GetCurrentState("Missing").empty());
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("UpperBody")->GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Translation.x, 2);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[1].Translation.y, 3);
    ASSERT_TRUE(instance.SetBool("Attack", false));
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_EQ(instance.GetCurrentState("UpperBody"), "Attack");
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetCurrentState("UpperBody"), "Idle");
    EXPECT_EQ(instance.GetCurrentState(), "Walk");
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0);
}

TEST_F(AnimationControllerTest, SharedParametersDriveAllMachinesAndResetRestartsThemTogether)
{
    auto info = MakeInfo();
    auto upper = info.Layers.front();
    upper.Name = "UpperBody";
    upper.Mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child");
    upper.Weight = 0.5f;
    info.Layers.push_back(upper);
    const auto controller = AnimationController::Create(info);
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh), other(Mesh);
    AnimationControllerInstance instance(controller, mesh), second(controller, other);
    ASSERT_TRUE(instance.SetFloat("Speed", 4));
    ASSERT_TRUE(instance.Update(0.75f));
    ASSERT_TRUE(second.Update(0.25f));
    EXPECT_EQ(instance.GetCurrentState(), "Run");
    EXPECT_EQ(instance.GetCurrentState("UpperBody"), "Run");
    EXPECT_EQ(second.GetCurrentState(), "Idle");
    EXPECT_EQ(second.GetCurrentState("UpperBody"), "Idle");
    EXPECT_FLOAT_EQ(other.GetTime(), 0.25f);
    instance.Reset();
    EXPECT_TRUE(instance.GetCurrentState().empty());
    EXPECT_TRUE(instance.GetCurrentState("UpperBody").empty());
    EXPECT_EQ(instance.GetFloat("Speed"), 0);
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    EXPECT_EQ(instance.GetCurrentState("UpperBody"), "Idle");
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.1f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("UpperBody")->GetTime(), 0.1f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("UpperBody")->GetWeight(), 0.5f);
    EXPECT_FLOAT_EQ(other.GetTime(), 0.25f);
}

TEST_F(AnimationControllerTest, ControllerConfiguresOrderedOverrideAndAdditiveLayersWithOwnedEvents)
{
    auto info = MakeInfo();
    const auto mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child");
    const auto makeClip = [&](const char* name, const float height)
    {
        return AnimationSequence::Create(name, Mesh->GetSkeleton(), 2, {
            { .BoneIndex = 1, .Translations = {{ 0, { 0, height, 0 } }} },
        }, {}, {{ "Event", 0.25f }});
    };
    const auto action = makeClip("Action", 3);
    const auto recoil = makeClip("Recoil", 2);
    auto reference = Mesh->GetData().ReferencePose;
    reference[1].Translation.y = 1;
    info.Layers.push_back({ .Name = "Action", .Mask = mask,
        .StateMachine = { .InitialState = "Active", .States = {{ "Active", action }} } });
    info.Layers.push_back({ .Name = "Recoil", .Mask = mask, .Weight = 0.5f,
        .BlendMode = EAnimationLayerBlendMode::Additive, .AdditiveReferencePose = reference,
        .StateMachine = { .InitialState = "Active", .States = {{ "Active", recoil }} } });
    const auto controller = AnimationController::Create(info);
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_EQ(mesh.GetLayerCount(), 3);
    EXPECT_EQ(mesh.GetLayer("Recoil")->GetBlendMode(), EAnimationLayerBlendMode::Additive);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[1].Translation.y, 3.5f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("Action")->GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("Recoil")->GetTime(), 0.5f);
    ASSERT_EQ(mesh.GetNotifyEvents().size(), 2);
    EXPECT_EQ(mesh.GetNotifyEvents()[0].LayerName, "Action");
    EXPECT_EQ(mesh.GetNotifyEvents()[1].LayerName, "Recoil");
    EXPECT_DOUBLE_EQ(mesh.GetNotifyEvents()[0].TimeOffset, 0.25);
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_TRUE(mesh.GetNotifyEvents().empty());
    ASSERT_TRUE(mesh.GetLayer("Recoil")->SetWeight(0));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[1].Translation.y, 3);
}

TEST_F(AnimationControllerTest, MissingLayerFailsBeforeAnyMachineOrClockChanges)
{
    auto info = MakeInfo();
    info.Layers.push_back({ .Name = "Action",
        .Mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child"),
        .StateMachine = info.Layers.front().StateMachine });
    const auto controller = AnimationController::Create(info);
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    EXPECT_FALSE(instance.Update(-1));
    EXPECT_EQ(mesh.GetLayerCount(), 1);
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(mesh.RemoveLayer("Action"));
    ASSERT_TRUE(instance.SetFloat("Speed", 4));
    EXPECT_FALSE(instance.Update(1));
    EXPECT_EQ(instance.GetCurrentState(), "Idle");
    EXPECT_EQ(instance.GetCurrentState("Action"), "Idle");
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.25f);
}

TEST_F(AnimationControllerTest, RejectsExistingLayersWithoutReplacingTheirPlayback)
{
    auto info = MakeInfo();
    const auto controller = AnimationController::Create(info);
    SkeletalMeshInstance mesh(Mesh);
    const auto mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child");
    ASSERT_TRUE(mesh.AddLayer("Existing", mask));
    ASSERT_TRUE(mesh.GetLayer("Existing")->Play(Walk));
    ASSERT_TRUE(mesh.Update(0.25f));
    AnimationControllerInstance instance(controller, mesh);
    EXPECT_FALSE(instance.Update(0.5f));
    EXPECT_EQ(mesh.GetLayerCount(), 2);
    EXPECT_FLOAT_EQ(mesh.GetLayer("Existing")->GetTime(), 0.25f);
    EXPECT_TRUE(instance.GetCurrentState().empty());
}

TEST_F(AnimationControllerTest, RejectsInvalidLayerDefinitionsAndCrossMachineSkeletons)
{
    auto valid = MakeInfo();
    valid.Layers.push_back({ .Name = "Action",
        .Mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child"),
        .StateMachine = valid.Layers.front().StateMachine });
    ASSERT_TRUE(AnimationController::Create(valid));
    auto info = valid;
    info.Layers.clear();
    EXPECT_FALSE(AnimationController::Create(info));
    info = valid;
    info.Layers[0].Name = "Other";
    EXPECT_FALSE(AnimationController::Create(info));
    info = valid;
    info.Layers[1].Name = "Base";
    EXPECT_FALSE(AnimationController::Create(info));
    info = valid;
    info.Layers.push_back(info.Layers.back());
    EXPECT_FALSE(AnimationController::Create(info));
    info = valid;
    info.Layers[1].Mask.reset();
    EXPECT_FALSE(AnimationController::Create(info));
    info = valid;
    info.Layers[0].Weight = 0.5f;
    EXPECT_FALSE(AnimationController::Create(info));
    info = valid;
    info.Layers[0].BlendMode = EAnimationLayerBlendMode::Additive;
    EXPECT_FALSE(AnimationController::Create(info));
    info = valid;
    info.Layers[1].Weight = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(AnimationController::Create(info));
    info = valid;
    info.Layers[1].BlendMode = EAnimationLayerBlendMode(255);
    EXPECT_FALSE(AnimationController::Create(info));
    info = valid;
    info.Layers[1].AdditiveReferencePose.resize(1);
    EXPECT_FALSE(AnimationController::Create(info));
    info = valid;
    info.Layers[1].AdditiveReferencePose = Mesh->GetData().ReferencePose;
    info.Layers[1].AdditiveReferencePose[0].Scale.x = 0;
    EXPECT_FALSE(AnimationController::Create(info));
    info = valid;
    info.Layers[1].StateMachine.InitialState = "Missing";
    EXPECT_FALSE(AnimationController::Create(info));
    info = valid;
    const auto other = Skeleton::Create("Other", {{ .Name = "Other" }});
    info.Layers[1].StateMachine.States = {{ "Other",
        AnimationSequence::Create("Other", other, 1, {}) }};
    info.Layers[1].StateMachine.InitialState = "Other";
    info.Layers[1].StateMachine.Transitions.clear();
    EXPECT_FALSE(AnimationController::Create(info));
    info = valid;
    info.Layers[1].Mask = BoneMask::FromBone("Other", other, "Other");
    EXPECT_FALSE(AnimationController::Create(info));
}

TEST_F(AnimationControllerTest, GlobalPauseAndIndividualLayerPauseKeepIndependentClocks)
{
    auto info = MakeInfo();
    info.Layers.push_back({ .Name = "Action",
        .Mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child"),
        .StateMachine = info.Layers.front().StateMachine });
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(AnimationController::Create(info), mesh);
    ASSERT_TRUE(instance.Update(0.25f));
    auto& action = *mesh.GetLayer("Action");
    action.Pause();
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(action.GetTime(), 0.25f);
    mesh.Pause();
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(action.GetTime(), 0.25f);
    mesh.Resume();
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.75f);
    EXPECT_FLOAT_EQ(action.GetTime(), 0.25f);
    action.Resume();
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 1);
    EXPECT_FLOAT_EQ(action.GetTime(), 0.5f);
}

TEST_F(AnimationControllerTest, FailedCompositionKeepsEveryLayerClockAndLastValidPose)
{
    auto info = MakeInfo();
    const auto mirror = AnimationSequence::Create("Mirror", Mesh->GetSkeleton(), 2, {
        { .BoneIndex = 1, .Scales = {{ 0, { -1, 1, 1 } }} },
    });
    info.Parameters.push_back({ "Attack", false });
    info.Layers.push_back({ .Name = "Action",
        .Mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child"),
        .StateMachine = {
            .InitialState = "Idle", .States = {{ "Idle", Idle }, { "Mirror", mirror }},
            .Transitions = {{ .From = "Idle", .To = "Mirror",
                .Conditions = {{ "Attack", EAnimationComparison::Equal, true }},
                .FadeDuration = 1 }},
        } });
    const auto controller = AnimationController::Create(info);
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(instance.SetBool("Attack", true));
    EXPECT_FALSE(instance.Update(0.5f));
    EXPECT_EQ(instance.GetCurrentState("Action"), "Mirror");
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("Action")->GetTime(), 0);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[1].Scale.x, 1);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("Action")->GetTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[1].Scale.x, 0.5f);
}

TEST_F(AnimationControllerTest, DirectSequenceStartsOnceHoldsItsEndAndRestartsOnReset)
{
    const auto sequence = Walk->WithNotifies({{ "Start", 0 }, { "Step", 0.25f }});
    const auto controller = AnimationController::Create({
        .Name = "DirectSequence",
        .Layers = {{ .Sequence = sequence, .Looping = false }},
    });
    ASSERT_TRUE(controller);
    EXPECT_TRUE(controller->GetLayers()[0].StateMachine.States.empty());
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_TRUE(instance.GetCurrentState().empty());
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Translation.x, 4);
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
    EXPECT_EQ(mesh.GetNotifyEvents()[0].GetNotify().Name, "Start");
}

TEST_F(AnimationControllerTest, DirectSequenceMatchesASingleStateMachineThroughLoopsAndPause)
{
    const auto direct = AnimationController::Create({
        .Name = "Direct", .Layers = {{ .Sequence = Walk }},
    });
    const auto machine = AnimationController::Create({
        .Name = "Machine", .Layers = {{ .StateMachine = {
            .InitialState = "Walk", .States = {{ "Walk", Walk }},
        } }},
    });
    ASSERT_TRUE(direct);
    ASSERT_TRUE(machine);
    SkeletalMeshInstance directMesh(Mesh), machineMesh(Mesh);
    AnimationControllerInstance first(direct, directMesh), second(machine, machineMesh);
    for (const auto dt : { 0.0f, 0.25f, 1.5f, 2.75f })
    {
        ASSERT_TRUE(first.Update(dt));
        ASSERT_TRUE(second.Update(dt));
        EXPECT_FLOAT_EQ(directMesh.GetTime(), machineMesh.GetTime());
        EXPECT_FLOAT_EQ(directMesh.GetLocalPose()[0].Translation.x,
            machineMesh.GetLocalPose()[0].Translation.x);
    }
    const auto time = directMesh.GetTime();
    directMesh.Pause();
    ASSERT_TRUE(first.Update(1));
    EXPECT_FLOAT_EQ(directMesh.GetTime(), time);
    directMesh.Resume();
    ASSERT_TRUE(first.Update(0.25f));
    EXPECT_FLOAT_EQ(directMesh.GetTime(), time + 0.25f);
}

TEST_F(AnimationControllerTest, DirectAndMachineSourcesShareParametersAndComposeInLayerOrder)
{
    auto info = MakeInfo();
    info.Layers[0].StateMachine = {};
    info.Layers[0].Sequence = Walk;
    const auto mask = BoneMask::FromBone("Child", Mesh->GetSkeleton(), "Child");
    const auto action = AnimationSequence::Create("Action", Mesh->GetSkeleton(), 2, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 3, 0 } }} },
    });
    const auto recoil = AnimationSequence::Create("Recoil", Mesh->GetSkeleton(), 2, {
        { .BoneIndex = 1, .Translations = {{ 0, { 0, 2, 0 } }} },
    }, {}, {{ "Recoil", 0.25f }});
    info.Layers.push_back({ .Name = "Action", .Mask = mask, .StateMachine = {
        .InitialState = "Idle", .States = {{ "Idle", Idle }, { "Attack", action }},
        .Transitions = {{ .From = "Idle", .To = "Attack", .Conditions = {
            { "Speed", EAnimationComparison::Greater, 0.1f } }, .FadeDuration = 0 }},
    } });
    info.Layers.push_back({ .Name = "Recoil", .Mask = mask, .Weight = 0.5f,
        .BlendMode = EAnimationLayerBlendMode::Additive, .Sequence = recoil });
    const auto controller = AnimationController::Create(info);
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.SetFloat("Speed", 1));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_TRUE(instance.GetCurrentState().empty());
    EXPECT_EQ(instance.GetCurrentState("Action"), "Attack");
    EXPECT_TRUE(instance.GetCurrentState("Recoil").empty());
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("Action")->GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLayer("Recoil")->GetTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Translation.x, 4);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[1].Translation.y, 4);
    ASSERT_EQ(mesh.GetNotifyEvents().size(), 1);
    EXPECT_EQ(mesh.GetNotifyEvents()[0].LayerName, "Recoil");
    ASSERT_TRUE(mesh.GetLayer("Recoil")->SetWeight(0));
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[1].Translation.y, 3);
    EXPECT_TRUE(mesh.GetNotifyEvents().empty());
}

TEST_F(AnimationControllerTest, DirectSourceValidationRejectsMissingAmbiguousAndMisplacedSettings)
{
    SAnimationControllerCreateInfo info{ .Name = "Direct", .Layers = {{ .Sequence = Walk }} };
    ASSERT_TRUE(AnimationController::Create(info));
    info.Layers[0].Sequence.reset();
    EXPECT_FALSE(AnimationController::Create(info));
    info.Layers[0].Sequence = Walk;
    info.Layers[0].StateMachine.InitialState = "Missing";
    EXPECT_FALSE(AnimationController::Create(info));
    info.Layers[0].StateMachine = MakeInfo().Layers[0].StateMachine;
    EXPECT_FALSE(AnimationController::Create(info));
    info.Layers[0].StateMachine = {};
    info.Layers[0].StateMachine.Transitions.push_back({ .From = "A", .To = "B" });
    EXPECT_FALSE(AnimationController::Create(info));
    info.Layers[0].StateMachine = {};
    info.Layers[0].BlendParameters = { "Speed" };
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Layers[0].BlendParameters = { "Speed" };
    EXPECT_FALSE(AnimationController::Create(info));
    info.Layers[0].BlendParameters.clear();
    info.Layers[0].Looping = false;
    EXPECT_FALSE(AnimationController::Create(info));
    info = { .Name = "Direct", .Layers = {{ .Sequence = Walk }} };
    const auto other = Skeleton::Create("Other", {{ .Name = "Other" }});
    info.Layers.push_back({ .Name = "Other",
        .Mask = BoneMask::FromBone("Other", other, "Other"),
        .Sequence = AnimationSequence::Create("Other", other, 1, {}) });
    EXPECT_FALSE(AnimationController::Create(info));
}
