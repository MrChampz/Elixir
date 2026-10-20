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
                .Name = "Locomotion", .InitialState = "Idle",
                .Parameters = {{ "Speed", 0.0f }, { "Grounded", true }},
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
                },
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
    EXPECT_EQ(controller->GetStates().size(), 3);
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
    info.Transitions = {
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
    info.InitialState = "Walk";
    info.States[1].Looping = false;
    info.Transitions = {{ .From = "Walk", .To = "Idle", .WaitForCompletion = true }};
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
    info.States[1].Looping = false;
    info.Transitions = {
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
    mesh.SetMesh(nullptr);
    EXPECT_FALSE(instance.Update(0.5f));
    AnimationControllerInstance missing(nullptr, mesh);
    EXPECT_FALSE(missing.Update(0.0f));
    EXPECT_FALSE(missing.SetFloat("Speed", 1.0f));
}

TEST_F(AnimationControllerTest, AcceptsCompatibleSkeletonsAndRestartsAfterMeshReplacement)
{
    const auto controller = AnimationController::Create(MakeInfo());
    auto other = SkinningTests::MakeMesh();
    ASSERT_NE(other->GetSkeleton(), Mesh->GetSkeleton());
    SkeletalMeshInstance mesh(other);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.5f));
    mesh.SetMesh(Mesh);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.25f);
    auto data = Mesh->GetData();
    data.Skeleton = Skeleton::Create("Other", {{ .Name = "Other" }, { .Name = "Child" }});
    auto incompatible = SkeletalMesh::Create(std::move(data));
    ASSERT_TRUE(incompatible);
    mesh.SetMesh(incompatible);
    EXPECT_FALSE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(mesh.GetTime(), 0.0f);
}

TEST_F(AnimationControllerTest, RejectsInvalidStatesParametersAndInitialState)
{
    auto info = MakeInfo();
    info.Name.clear();
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.InitialState = "Missing";
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.States.clear();
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.States[1].Name = "Idle";
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.States[1].Sequence.reset();
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.States[1].Sequence = AnimationSequence::Create("Other",
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
    info.Transitions[0].To = "Missing";
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Transitions[0].To = "Idle";
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Transitions[0].From = "Missing";
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Transitions[0].FadeDuration = -1.0f;
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Transitions[0].FadeDuration = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Transitions[0].WaitForCompletion = true;
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Transitions[0].Conditions[0].Parameter = "Missing";
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Transitions[0].Conditions[0].Value = true;
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Transitions[0].Conditions[1].Comparison = EAnimationComparison::Greater;
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Transitions[0].Conditions[0].Comparison = EAnimationComparison(255);
    EXPECT_FALSE(AnimationController::Create(info));
    info = MakeInfo();
    info.Transitions[0].Conditions[0].Value = std::numeric_limits<float>::infinity();
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
            info.Transitions = {{ .From = "Idle", .To = "Walk",
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
    info.Transitions = {{ .From = "Idle", .To = "Walk",
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
    info.States[1].Sequence = AnimationSequence::Create("Mirror", Mesh->GetSkeleton(), 2, {
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
