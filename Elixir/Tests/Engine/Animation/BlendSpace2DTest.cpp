#include <gtest/gtest.h>
#include <Engine/Animation/AnimationControllerInstance.h>
#include "../Mesh/SkinningTestData.h"
#include <limits>

using namespace Elixir;

namespace
{
    class BlendSpace2DTest : public testing::Test
    {
      protected:
        /** @brief Create three cycles with distinct durations and affine translations. */
        void SetUp() override
        {
            Mesh = SkinningTests::MakeMesh();
            ASSERT_TRUE(Mesh);
            for (size_t i = 0; i < 3; ++i)
                Clips[i] = AnimationSequence::Create(
                    std::to_string(i), Mesh->GetSkeleton(), float(i + 1),
                    {{.BoneIndex = 0,
                      .Translations = {
                          {0, {float(i * 10), 0, 0}},
                          {float(i + 1), {float(i * 10 + 4), 0, 0}},
                      }}}
                );
            Space = BlendSpace2D::Create(
                "Plane", {
                             {{0, 0}, Clips[0]},
                             {{2, 0}, Clips[1]},
                             {{0, 2}, Clips[2]},
                         }
            );
            ASSERT_TRUE(Space);
        }

        /** @brief Define a controller with independent lateral and forward parameters. */
        SAnimationControllerCreateInfo Definition() const
        {
            return {
                .Name = "Controller",
                .InitialState = "Move",
                .Parameters = {{"X", 0.5f}, {"Y", 0.5f}},
                .States = {
                    {.Name = "Move",
                     .BlendParameter = "X",
                     .Space2D = Space,
                     .BlendParameterY = "Y"}
                }
            };
        }

        Ref<SkeletalMesh> Mesh;
        std::array<Ref<AnimationSequence>, 3> Clips;
        Ref<BlendSpace2D> Space;
    };
}

TEST_F(BlendSpace2DTest, BarycentricWeightsBlendThreePhaseAlignedCycles)
{
    EXPECT_EQ(Space->GetName(), "Plane");
    EXPECT_EQ(Space->GetSkeleton(), Mesh->GetSkeleton());
    std::vector<SBoneTransform> pose;
    ASSERT_TRUE(Space->Sample({0.5f, 0.5f}, 0.25f, pose));
    EXPECT_NEAR(pose[0].Translation.x, 8.5f, 1e-5f);
    EXPECT_FLOAT_EQ(Space->GetDuration({0.5f, 0.5f}), 1.75f);
    const auto weights = Space->GetWeights({0.5f, 0.5f});
    glm::vec2 reconstructed(0);
    float sum = 0;
    for (size_t i = 0; i < 3; ++i)
    {
        EXPECT_GE(weights.Weights[i], 0.0f);
        reconstructed += Space->GetSamples()[weights.Samples[i]].Position * weights.Weights[i];
        sum += weights.Weights[i];
    }
    EXPECT_FLOAT_EQ(sum, 1);
    EXPECT_EQ(reconstructed, glm::vec2(0.5f));
}

TEST_F(BlendSpace2DTest, ExactSamplesEdgesAndOutsideHullUseNearestBoundary)
{
    std::vector<SBoneTransform> pose;
    ASSERT_TRUE(Space->Sample({2, 0}, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 12);
    ASSERT_TRUE(Space->Sample({0, 2}, 2, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 24);
    ASSERT_TRUE(Space->Sample({-5, -5}, -1, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 0);
    ASSERT_TRUE(Space->Sample({10, 10}, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 17);
    ASSERT_TRUE(Space->Sample({1, -10}, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 7);
}

TEST_F(BlendSpace2DTest, CocircularGridAndCollinearBoundaryPointsRemainUsable)
{
    std::vector<SBlendSpace2DSample> samples;
    for (int x = -2; x <= 2; ++x)
        for (int y = -2; y <= 2; ++y)
            samples.push_back({{float(x), float(y)}, Clips[size_t((x + y + 4) % 3)]});
    const auto space = BlendSpace2D::Create("Grid", samples);
    ASSERT_TRUE(space);
    std::ranges::reverse(samples);
    const auto reversed = BlendSpace2D::Create("Reverse", samples);
    ASSERT_TRUE(reversed);
    for (const auto& sample : samples)
    {
        EXPECT_FLOAT_EQ(space->GetDuration(sample.Position), sample.Sequence->GetDuration());
        EXPECT_FLOAT_EQ(
            reversed->GetDuration(sample.Position), sample.Sequence->GetDuration()
        );
    }
    for (float x = -3; x < 3; x += 0.17f)
        EXPECT_FLOAT_EQ(space->GetDuration({x, 0.37f}), reversed->GetDuration({x, 0.37f}));
}

TEST_F(BlendSpace2DTest, RejectsInvalidAssetsAndPreservesOutputOnInvalidSampling)
{
    EXPECT_FALSE(BlendSpace2D::Create("", Space->GetSamples()));
    EXPECT_FALSE(BlendSpace2D::Create("Empty", {}));
    EXPECT_FALSE(
        BlendSpace2D::Create(
            "Line", {
                        {{0, 0}, Clips[0]},
                        {{1, 0}, Clips[1]},
                        {{2, 0}, Clips[2]},
                    }
        )
    );
    auto samples = Space->GetSamples();
    samples[0].Position = samples[1].Position;
    EXPECT_FALSE(BlendSpace2D::Create("Duplicate", samples));
    samples = Space->GetSamples();
    samples[0].Sequence.reset();
    EXPECT_FALSE(BlendSpace2D::Create("Missing", samples));
    samples[0].Sequence = AnimationSequence::Create(
        "Other", Skeleton::Create("Other", {{.Name = "Other"}}), 0, {}
    );
    EXPECT_FALSE(BlendSpace2D::Create("Incompatible", samples));
    samples = Space->GetSamples();
    samples[0].Position.x = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(BlendSpace2D::Create("Infinite", samples));
    auto pose = Mesh->GetData().ReferencePose;
    pose[0].Translation.x = 99;
    EXPECT_FALSE(Space->Sample({0, 0}, std::numeric_limits<float>::quiet_NaN(), pose));
    EXPECT_FALSE(Space->Sample({std::numeric_limits<float>::infinity(), 0}, 0, pose));
    EXPECT_FALSE(Space->Sample({0, 0}, 0, pose, std::span(pose).first(1)));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 99);
}

TEST_F(BlendSpace2DTest, HandlesWideFiniteCoordinatesAndCycleDurations)
{
    const auto maximum = std::numeric_limits<float>::max();
    const auto space = BlendSpace2D::Create(
        "Wide", {
                    {{-maximum, -maximum}, Clips[0]},
                    {{maximum, -maximum}, Clips[1]},
                    {{0, maximum}, Clips[2]},
                }
    );
    ASSERT_TRUE(space);
    EXPECT_FLOAT_EQ(space->GetDuration({0, 0}), 2.25f);
    const auto clip = AnimationSequence::Create("Long", Mesh->GetSkeleton(), maximum, {});
    const auto longSpace = BlendSpace2D::Create(
        "Long", {
                    {{0, 0}, clip},
                    {{2, 0}, clip},
                    {{0, 2}, clip},
                }
    );
    ASSERT_TRUE(longSpace);
    EXPECT_FLOAT_EQ(longSpace->GetDuration({0.37f, 0.71f}), maximum);
}

TEST_F(BlendSpace2DTest, BlendsRotationsAndScaleAndRejectsSingularFinalTransforms)
{
    const auto rotation = glm::angleAxis(glm::radians(120.0f), glm::vec3(0, 0, 1));
    const auto turn = AnimationSequence::Create(
        "Turn", Mesh->GetSkeleton(), 1,
        {
            {.BoneIndex = 0, .Rotations = {{0, -rotation}}, .Scales = {{0, {3, 3, 3}}}},
        }
    );
    const auto space = BlendSpace2D::Create(
        "Turn", {
                    {{0, 0}, Clips[0]},
                    {{2, 0}, turn},
                    {{0, 2}, Clips[0]},
                }
    );
    auto reference = Mesh->GetData().ReferencePose;
    reference[1].Translation.y = 7;
    std::vector<SBoneTransform> pose;
    ASSERT_TRUE(space->Sample({1, 0.5f}, 0, pose, reference));
    EXPECT_FLOAT_EQ(pose[0].Scale.x, 2);
    EXPECT_FLOAT_EQ(pose[1].Translation.y, 7);
    EXPECT_NEAR(glm::length(pose[0].Rotation), 1, 1e-5f);
    const auto expected = glm::angleAxis(glm::radians(60.0f), glm::vec3(0, 0, 1));
    EXPECT_NEAR(std::abs(glm::dot(pose[0].Rotation, expected)), 1, 1e-5f);
    const auto mirror = AnimationSequence::Create(
        "Mirror", Mesh->GetSkeleton(), 0,
        {
            {.BoneIndex = 0, .Scales = {{0, {-1, 1, 1}}}},
        }
    );
    const auto singular = BlendSpace2D::Create(
        "Singular", {
                        {{0, 0}, Clips[0]},
                        {{2, 0}, mirror},
                        {{0, 2}, Clips[0]},
                    }
    );
    pose[0].Translation.x = 99;
    EXPECT_FALSE(singular->Sample({1, 0}, 0, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 99);
}

TEST_F(BlendSpace2DTest, PlaybackRetainsPhaseAcrossCoordinateChangesPauseAndSeek)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.PlayBlendSpace(Space, glm::vec2(0.5f)));
    ASSERT_TRUE(instance.Update(0.4375f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    ASSERT_TRUE(instance.SetBlendSpaceValue(glm::vec2(0, 2)));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.75f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 21);
    instance.Pause();
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    instance.Resume();
    ASSERT_TRUE(instance.Seek(-0.75f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.75f);
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    ASSERT_TRUE(instance.PlayBlendSpace(Space, glm::vec2(0, 2), 0, false));
    ASSERT_TRUE(instance.Update(10));
    EXPECT_FALSE(instance.IsPlaying());
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 1);
}

TEST_F(BlendSpace2DTest, FadesAcrossAllSourceTypesAndRetainsVisiblePoseOnInterruption)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Clips[0]));
    ASSERT_TRUE(instance.PlayBlendSpace(Space, glm::vec2(0, 2), 1));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_NEAR(instance.GetLocalPose()[0].Translation.x, 11.333333f, 1e-5f);
    const auto visible = instance.GetLocalPose()[0].Translation.x;
    const auto one = BlendSpace1D::Create("Line", {{0, Clips[0]}, {1, Clips[1]}});
    ASSERT_TRUE(instance.PlayBlendSpace(one, 1.0f, 1));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, visible);
    ASSERT_TRUE(instance.Update(1));
    ASSERT_TRUE(instance.PlayBlendSpace(Space, glm::vec2(0.5f), 1));
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FALSE(instance.IsTransitioning());
    ASSERT_TRUE(instance.Play(Clips[0], 1.0f));
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FALSE(instance.IsTransitioning());
}

TEST_F(BlendSpace2DTest, ControllerBindsBothAxesWithoutRestartingAndRejectsInvalidBindings)
{
    auto info = Definition();
    const auto controller = AnimationController::Create(info);
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.4375f));
    ASSERT_TRUE(instance.SetFloat("X", 0));
    ASSERT_TRUE(instance.SetFloat("Y", 2));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(mesh.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Translation.x, 21);
    info.States[0].BlendParameterY = "Missing";
    EXPECT_FALSE(AnimationController::Create(info));
    info = Definition();
    info.Parameters[1].DefaultValue = true;
    EXPECT_FALSE(AnimationController::Create(info));
    info = Definition();
    info.States[0].Sequence = Clips[0];
    EXPECT_FALSE(AnimationController::Create(info));
    info = Definition();
    info.States[0].Space2D.reset();
    info.States[0].BlendSpace = BlendSpace1D::Create("Line", {{0, Clips[0]}});
    EXPECT_FALSE(AnimationController::Create(info));
}

TEST_F(BlendSpace2DTest, InvalidPlaybackRequestsLeaveTheCurrentPoseAndPhaseIntact)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.PlayBlendSpace(Space, glm::vec2(0.5f)));
    ASSERT_TRUE(instance.Update(0.4375f));
    const auto invalid = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(instance.PlayBlendSpace(Ref<const BlendSpace2D>{}, glm::vec2(0)));
    EXPECT_FALSE(instance.PlayBlendSpace(Space, glm::vec2(invalid, 0)));
    EXPECT_FALSE(instance.PlayBlendSpace(Space, glm::vec2(0), -1));
    EXPECT_FALSE(instance.SetBlendSpaceValue(glm::vec2(0, invalid)));
    EXPECT_FALSE(instance.SetBlendSpaceValue(0.0f));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 8.5f);
    instance.Stop();
    EXPECT_FALSE(instance.SetBlendSpaceValue(glm::vec2(0)));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0);
    instance.SetMesh(nullptr);
    EXPECT_FALSE(instance.PlayBlendSpace(Space, glm::vec2(0)));
}

TEST_F(BlendSpace2DTest, StaticCyclesStayResponsiveAndCharactersKeepIndependentCoordinates)
{
    const auto idle = AnimationSequence::Create("Idle", Mesh->GetSkeleton(), 0, {});
    const auto space = BlendSpace2D::Create(
        "StaticOrigin", {
                            {{0, 0}, idle},
                            {{1, 0}, Clips[0]},
                            {{0, 1}, Clips[1]},
                        }
    );
    SkeletalMeshInstance first(Mesh), second(Mesh);
    ASSERT_TRUE(first.PlayBlendSpace(space, glm::vec2(0)));
    ASSERT_TRUE(second.PlayBlendSpace(space, glm::vec2(0, 1)));
    ASSERT_TRUE(first.Update(100));
    EXPECT_TRUE(first.IsPlaying());
    EXPECT_FLOAT_EQ(first.GetNormalizedTime(), 0);
    ASSERT_TRUE(first.SetBlendSpaceValue(glm::vec2(1, 0)));
    ASSERT_TRUE(first.Update(0.25f));
    ASSERT_TRUE(second.Update(1));
    EXPECT_FLOAT_EQ(first.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(second.GetNormalizedTime(), 0.5f);
    EXPECT_FLOAT_EQ(first.GetLocalPose()[0].Translation.x, 1);
    EXPECT_FLOAT_EQ(second.GetLocalPose()[0].Translation.x, 12);
    ASSERT_TRUE(first.PlayBlendSpace(space, glm::vec2(0), 0, false));
    EXPECT_FLOAT_EQ(first.GetNormalizedTime(), 1);
    EXPECT_FALSE(first.IsPlaying());
}

TEST_F(BlendSpace2DTest, ControllerTransitionsToSequenceAndBackToBothLiveAxes)
{
    auto info = Definition();
    info.Parameters.push_back({"Attack", false});
    info.States.push_back({.Name = "Attack", .Sequence = Clips[0], .Looping = false});
    info.Transitions = {
        {.From = "Move",
         .To = "Attack",
         .Conditions = {{"Attack", EAnimationComparison::Equal, true}},
         .FadeDuration = 0.2f},
        {.From = "Attack", .To = "Move", .WaitForCompletion = true},
    };
    const auto controller = AnimationController::Create(info);
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(0.4375f));
    ASSERT_TRUE(instance.SetBool("Attack", true));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetCurrentState(), "Attack");
    EXPECT_TRUE(mesh.IsTransitioning());
    ASSERT_TRUE(instance.SetBool("Attack", false));
    ASSERT_TRUE(instance.SetFloat("X", 0));
    ASSERT_TRUE(instance.SetFloat("Y", 2));
    ASSERT_TRUE(instance.Update(1));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetCurrentState(), "Move");
    ASSERT_TRUE(instance.Update(0.3f));
    EXPECT_FALSE(mesh.IsTransitioning());
    EXPECT_NEAR(mesh.GetLocalPose()[0].Translation.x, 20.4f, 1e-5f);
}
