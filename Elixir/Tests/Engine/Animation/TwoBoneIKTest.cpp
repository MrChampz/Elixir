#include <gtest/gtest.h>
#include "../Mesh/SkinningTestData.h"
#include <limits>

using namespace Elixir;

namespace
{
    /** @brief Create a straight two-segment limb, a tip child, and a root sibling. */
    Ref<SkeletalMesh> MakeIKMesh()
    {
        auto data = SkinningTests::MakeMesh()->GetData();
        data.Skeleton = Skeleton::Create("Limb", {
            {.Name = "Root"},
            {.Name = "Middle", .ParentIndex = 0,
                .ReferenceTransform = {.Translation = {1, 0, 0}}},
            {.Name = "End", .ParentIndex = 1,
                .ReferenceTransform = {.Translation = {1, 0, 0}}},
            {.Name = "Finger", .ParentIndex = 2,
                .ReferenceTransform = {.Translation = {0.2f, 0, 0}}},
            {.Name = "Sibling", .ParentIndex = 0,
                .ReferenceTransform = {.Translation = {0, 1, 0}}}
        }, {{.Name = "Grip", .BoneIndex = 2}});
        data.ReferencePose.clear();
        return SkeletalMesh::Create(std::move(data));
    }

    /**
     * @brief Compare a position with a tolerance suitable for float pose matrices.
     * @param actual Solved position.
     * @param expected Target position.
     */
    void ExpectPosition(const glm::vec3& actual, const glm::vec3& expected)
    {
        EXPECT_NEAR(actual.x, expected.x, 1e-4f);
        EXPECT_NEAR(actual.y, expected.y, 1e-4f);
        EXPECT_NEAR(actual.z, expected.z, 1e-4f);
    }

    /**
     * @brief Configure one limb target on a mesh instance.
     * @param instance Instance to configure.
     * @param target Skeleton-space destination.
     * @param pole Skeleton-space bend point.
     * @param weight Rotation blend weight.
     */
    bool SetTarget(SkeletalMeshInstance& instance, glm::vec3 target,
        glm::vec3 pole = {0, 2, 0}, float weight = 1)
    {
        const STwoBoneIKConstraint constraint{2, target, pole, weight};
        return instance.SetTwoBoneIKConstraints(std::span(&constraint, 1));
    }
}

TEST(TwoBoneIKTest, ReachesTargetPreservesLengthsAndUpdatesSocketsAndDescendants)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    ASSERT_TRUE(SetTarget(instance, {1, 1, 0}));
    EXPECT_FLOAT_EQ(instance.GetModelPose()[2][3].x, 2);
    ASSERT_TRUE(instance.Update(0));
    const auto& model = instance.GetModelPose();
    ExpectPosition(glm::vec3(model[2][3]), {1, 1, 0});
    ExpectPosition(glm::vec3(model[1][3]), {0, 1, 0});
    EXPECT_NEAR(glm::length(glm::vec3(model[1][3] - model[0][3])), 1, 1e-5f);
    EXPECT_NEAR(glm::length(glm::vec3(model[2][3] - model[1][3])), 1, 1e-5f);
    ExpectPosition(glm::vec3((*instance.GetSocketTransform("Grip"))[3]), {1, 1, 0});
    ExpectPosition(glm::vec3(model[3][3]), {1.2f, 1, 0});
    ExpectPosition(glm::vec3(model[4][3]), {-1, 0, 0});
    for (size_t i = 1; i < instance.GetLocalPose().size(); ++i)
        ExpectPosition(instance.GetLocalPose()[i].Translation,
            instance.GetMesh()->GetData().ReferencePose[i].Translation);
}

TEST(TwoBoneIKTest, PoleSelectsOppositeBend)
{
    SkeletalMeshInstance first(MakeIKMesh()), second(first.GetMesh());
    ASSERT_TRUE(SetTarget(first, {1, 1, 0}, {0, 2, 0}));
    ASSERT_TRUE(SetTarget(second, {1, 1, 0}, {2, 0, 0}));
    ASSERT_TRUE(first.Update(0));
    ASSERT_TRUE(second.Update(0));
    ExpectPosition(glm::vec3(first.GetModelPose()[1][3]), {0, 1, 0});
    ExpectPosition(glm::vec3(second.GetModelPose()[1][3]), {1, 0, 0});
    ExpectPosition(glm::vec3(second.GetModelPose()[2][3]), {1, 1, 0});
}

TEST(TwoBoneIKTest, ZeroWeightIsExactAndPartialWeightBlendsLocalRotations)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    const auto initial = instance.GetModelPose();
    ASSERT_TRUE(SetTarget(instance, {1, 1, 0}, {0, 2, 0}, 0));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetModelPose(), initial);
    ASSERT_TRUE(SetTarget(instance, {1, 1, 0}, {0, 2, 0}, 0.5f));
    ASSERT_TRUE(instance.Update(0));
    const auto angle = glm::angle(instance.GetLocalPose()[0].Rotation);
    EXPECT_NEAR(angle, glm::radians(45.0f), 1e-5f);
    ExpectPosition(glm::vec3(instance.GetModelPose()[2][3]),
        {1 + std::sqrt(0.5f), std::sqrt(0.5f), 0});
}

TEST(TwoBoneIKTest, FarTargetsClampWithoutStretching)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    ASSERT_TRUE(SetTarget(instance, {0, 100, 0}));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[2][3]), {0, 2, 0});
}

TEST(TwoBoneIKTest, NearTargetsClampToUnequalSegmentReach)
{
    auto data = MakeIKMesh()->GetData();
    data.ReferencePose[1].Translation.x = 2;
    SkeletalMeshInstance instance(SkeletalMesh::Create(std::move(data)));
    ASSERT_TRUE(SetTarget(instance, {0.1f, 0, 0}));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[2][3]), {1, 0, 0});
}

TEST(TwoBoneIKTest, CoincidentTargetAndDegeneratePolesRemainFinite)
{
    for (const auto target : {glm::vec3(0), glm::vec3(-1, 0, 0), glm::vec3(2, 0, 0)})
    {
        SkeletalMeshInstance instance(MakeIKMesh());
        ASSERT_TRUE(SetTarget(instance, target, {0, 0, 0}));
        ASSERT_TRUE(instance.Update(0));
        ExpectPosition(glm::vec3(instance.GetModelPose()[2][3]), target);
        for (const auto& transform : instance.GetLocalPose()) EXPECT_TRUE(transform.IsValid());
    }
}

TEST(TwoBoneIKTest, SupportsAThreeDimensionalLegTarget)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    ASSERT_TRUE(SetTarget(instance, {0, -1, 1}, {1, -1, 0}));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[2][3]), {0, -1, 1});
    EXPECT_GT(instance.GetModelPose()[1][3].x, 0);
}

TEST(TwoBoneIKTest, RespectsRotatedUniformlyScaledAncestors)
{
    auto data = MakeIKMesh()->GetData();
    auto bones = data.Skeleton->GetBones();
    bones.insert(bones.begin(), {.Name = "Wrapper", .ReferenceTransform = {
        .Translation = {3, 4, 5},
        .Rotation = glm::angleAxis(glm::radians(60.0f), glm::vec3(0, 0, 1)),
        .Scale = {2, 2, 2}}});
    for (size_t i = 1; i < bones.size(); ++i) ++bones[i].ParentIndex;
    data.Skeleton = Skeleton::Create("Wrapped", std::move(bones));
    data.ReferencePose.clear();
    SkeletalMeshInstance instance(SkeletalMesh::Create(std::move(data)));
    const auto target = glm::vec3(instance.GetModelPose()[0] * glm::vec4(1, 1, 0, 1));
    const auto pole = glm::vec3(instance.GetModelPose()[0] * glm::vec4(0, 2, 0, 1));
    const STwoBoneIKConstraint constraint{3, target, pole};
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[3][3]), target);
}

TEST(TwoBoneIKTest, RejectsInvalidSettingsAndPreservesConfiguration)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    ASSERT_TRUE(SetTarget(instance, {1, 1, 0}));
    for (const auto weight : {-1.0f, 1.1f, std::numeric_limits<float>::quiet_NaN()})
        EXPECT_FALSE(SetTarget(instance, {1, 0, 0}, {0, 1, 0}, weight));
    EXPECT_FALSE(SetTarget(instance, {std::numeric_limits<float>::infinity(), 0, 0}));
    EXPECT_FALSE(SetTarget(instance, {}, {0, std::numeric_limits<float>::quiet_NaN(), 0}));
    for (const auto bone : {0u, 1u, 20u})
    {
        const STwoBoneIKConstraint constraint{.EndBoneIndex = bone};
        EXPECT_FALSE(instance.SetTwoBoneIKConstraints(std::span(&constraint, 1)));
    }
    ASSERT_EQ(instance.GetTwoBoneIKConstraints().size(), 1);
    ExpectPosition(instance.GetTwoBoneIKConstraints()[0].TargetPosition, {1, 1, 0});
    EXPECT_TRUE(instance.SetTwoBoneIKConstraints(instance.GetTwoBoneIKConstraints()));
    SkeletalMeshInstance empty(nullptr);
    EXPECT_FALSE(SetTarget(empty, {1, 0, 0}));
    EXPECT_TRUE(empty.SetTwoBoneIKConstraints({}));
}

TEST(TwoBoneIKTest, FailedEvaluationPreservesPlaybackPoseAndNotifyBatch)
{
    const auto mesh = MakeIKMesh();
    const auto sequence = AnimationSequence::Create("Scale", mesh->GetSkeleton(), 1, {{
        .BoneIndex = 1,
        .Scales = {{0, {1, 1, 1}}, {1, {2, 1, 1}}}
    }}, {}, {{"Event", 0.1f}});
    SkeletalMeshInstance instance(mesh);
    ASSERT_TRUE(instance.Play(sequence));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    ASSERT_TRUE(SetTarget(instance, {1, 1, 0}));
    const auto pose = instance.GetModelPose();
    EXPECT_FALSE(instance.Update(0.2f));
    EXPECT_EQ(instance.GetModelPose(), pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.1f);
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints({}));
    EXPECT_TRUE(instance.Update(0.2f));
}

TEST(TwoBoneIKTest, RejectsZeroLengthsAndNegativeScalesOnlyWhenActive)
{
    for (const auto scale : {glm::vec3(-1), glm::vec3(2, 1, 1)})
    {
        auto data = MakeIKMesh()->GetData();
        data.ReferencePose[0].Scale = scale;
        SkeletalMeshInstance instance(SkeletalMesh::Create(std::move(data)));
        ASSERT_TRUE(SetTarget(instance, {1, 1, 0}));
        EXPECT_FALSE(instance.Update(0));
        ASSERT_TRUE(SetTarget(instance, {1, 1, 0}, {0, 1, 0}, 0));
        EXPECT_TRUE(instance.Update(0));
    }
    auto data = MakeIKMesh()->GetData();
    data.ReferencePose[1].Translation = {};
    SkeletalMeshInstance instance(SkeletalMesh::Create(std::move(data)));
    ASSERT_TRUE(SetTarget(instance, {1, 1, 0}));
    EXPECT_FALSE(instance.Update(0));
}

TEST(TwoBoneIKTest, ReadsAfterAdditiveLayersAndMontagesWithoutChangingTheirPlayback)
{
    const auto mesh = MakeIKMesh();
    SkeletalMeshInstance instance(mesh);
    const auto sequence = AnimationSequence::Create("Lift", mesh->GetSkeleton(), 1, {{
        .BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {0, 1, 0}}}
    }});
    ASSERT_TRUE(instance.AddLayer("Action", BoneMask::FromBone("All", mesh->GetSkeleton(), "Root")));
    auto* layer = instance.GetLayer("Action");
    ASSERT_TRUE(layer->SetBlendMode(EAnimationLayerBlendMode::Additive));
    ASSERT_TRUE(layer->PlayMontage(AnimationMontage::Create({.Name = "Lift", .Sequence = sequence,
        .BlendIn = 0, .BlendOut = 0})));
    ASSERT_TRUE(SetTarget(instance, {1, 1, 0}));
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(glm::vec3(instance.GetModelPose()[2][3]), {1, 1, 0});
    EXPECT_FLOAT_EQ(layer->GetMontageTime(), 0.5f);
    EXPECT_FLOAT_EQ(instance.GetModelPose()[0][3].y, 0.5f);
}

TEST(TwoBoneIKTest, UpdatingPausedTargetsDoesNotAccumulateCorrections)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    ASSERT_TRUE(SetTarget(instance, {1, 1, 0}, {0, 2, 0}, 0.5f));
    instance.Pause();
    ASSERT_TRUE(instance.Update(0));
    const auto pose = instance.GetModelPose();
    ASSERT_TRUE(instance.Update(1));
    EXPECT_EQ(instance.GetModelPose(), pose);
    ASSERT_TRUE(SetTarget(instance, {0, 1, 0}));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[2][3]), {0, 1, 0});
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints({}));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[2][3]), {2, 0, 0});
}

TEST(TwoBoneIKTest, ManualPoseIsSolvedAndStopRestoresReferencePose)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    ASSERT_TRUE(SetTarget(instance, {1, 1, 0}));
    ASSERT_TRUE(instance.SetLocalPose(instance.GetLocalPose()));
    ExpectPosition(glm::vec3(instance.GetModelPose()[2][3]), {1, 1, 0});
    instance.Stop();
    EXPECT_TRUE(instance.GetTwoBoneIKConstraints().empty());
    ExpectPosition(glm::vec3(instance.GetModelPose()[2][3]), {2, 0, 0});
}

TEST(TwoBoneIKTest, LaterConstraintsUseThePoseProducedByEarlierOnes)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    const std::array<STwoBoneIKConstraint, 2> constraints = {{
        {2, {1, 1, 0}, {0, 2, 0}}, {2, {1, -1, 0}, {0, -2, 0}}
    }};
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(constraints));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[2][3]), {1, -1, 0});
}

TEST(TwoBoneIKTest, ProtectsTheExtractedRootMotionFrame)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    ASSERT_TRUE(instance.SetRootMotionEnabled(true));
    ASSERT_TRUE(SetTarget(instance, {1, 1, 0}));
    EXPECT_FALSE(instance.Update(0));
    ASSERT_TRUE(instance.SetRootMotionEnabled(false));
    ASSERT_TRUE(instance.Update(0));
    const auto pose = instance.GetModelPose();
    EXPECT_FALSE(instance.SetRootMotionEnabled(true));
    EXPECT_FALSE(instance.IsRootMotionEnabled());
    EXPECT_EQ(instance.GetModelPose(), pose);
}

namespace
{
    /**
     * @brief Compare skeleton-space rotation axes without including inherited scale.
     * @param actual Skeleton-space bone matrix.
     * @param expected Desired unit quaternion.
     */
    void ExpectRotation(const glm::mat4& actual, const glm::quat& expected)
    {
        const auto rotation = glm::mat3_cast(expected);
        for (int axis = 0; axis < 3; ++axis)
            ExpectPosition(glm::normalize(glm::vec3(actual[axis])), rotation[axis]);
    }
}

TEST(TwoBoneIKTest, TargetRotationAlignsTheEndWithoutMovingItsPosition)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    const auto rotation = glm::angleAxis(0.8f, glm::normalize(glm::vec3(1, 2, 3)));
    const STwoBoneIKConstraint constraint{2, {1, 1, 0}, {0, 2, 0}, 1, rotation};
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    const auto& model = instance.GetModelPose();
    ExpectPosition(glm::vec3(model[2][3]), {1, 1, 0});
    ExpectRotation(model[2], rotation);
    ExpectRotation(*instance.GetSocketTransform("Grip"), rotation);
    ExpectPosition(glm::vec3(model[3][3]), glm::vec3(1, 1, 0) + rotation * glm::vec3(0.2f, 0, 0));
}

TEST(TwoBoneIKTest, AbsentRotationPreservesAnimatedEndLocalRotation)
{
    auto data = MakeIKMesh()->GetData();
    const auto rotation = glm::angleAxis(0.6f, glm::vec3(1, 0, 0));
    data.ReferencePose[2].Rotation = rotation;
    SkeletalMeshInstance instance(SkeletalMesh::Create(std::move(data)));
    ASSERT_TRUE(SetTarget(instance, {1, 1, 0}));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetLocalPose()[2].Rotation, rotation);
}

TEST(TwoBoneIKTest, PartialWeightBlendsFromThePositionCorrectedOrientation)
{
    const auto mesh = MakeIKMesh();
    SkeletalMeshInstance positionOnly(mesh), oriented(mesh);
    const auto rotation = glm::angleAxis(1.4f, glm::vec3(0, 0, 1));
    ASSERT_TRUE(SetTarget(positionOnly, {1, 1, 0}, {0, 2, 0}, 0.5f));
    ASSERT_TRUE(positionOnly.Update(0));
    const auto initialRotation = glm::quat_cast(glm::mat3(positionOnly.GetModelPose()[2]));
    const STwoBoneIKConstraint constraint{2, {1, 1, 0}, {0, 2, 0}, 0.5f, rotation};
    ASSERT_TRUE(oriented.SetTwoBoneIKConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(oriented.Update(0));
    ExpectRotation(oriented.GetModelPose()[2], glm::slerp(initialRotation, rotation, 0.5f));
    ExpectPosition(glm::vec3(oriented.GetModelPose()[2][3]),
        glm::vec3(positionOnly.GetModelPose()[2][3]));
}

TEST(TwoBoneIKTest, ZeroWeightPreservesTheEntirePoseWithATargetRotation)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    const auto pose = instance.GetModelPose();
    const STwoBoneIKConstraint constraint{2, {1, 1, 0}, {0, 2, 0}, 0,
        glm::angleAxis(1.2f, glm::vec3(0, 1, 0))};
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetModelPose(), pose);
}

TEST(TwoBoneIKTest, OppositeQuaternionSignsGiveTheSameShortestPath)
{
    const auto mesh = MakeIKMesh();
    SkeletalMeshInstance first(mesh), second(mesh);
    const auto rotation = glm::angleAxis(0.8f, glm::normalize(glm::vec3(1, 2, 3)));
    const STwoBoneIKConstraint positive{2, {1, 1, 0}, {0, 2, 0}, 0.5f, rotation};
    const STwoBoneIKConstraint negative{2, {1, 1, 0}, {0, 2, 0}, 0.5f, -rotation};
    ASSERT_TRUE(first.SetTwoBoneIKConstraints(std::span(&positive, 1)));
    ASSERT_TRUE(second.SetTwoBoneIKConstraints(std::span(&negative, 1)));
    ASSERT_TRUE(first.Update(0));
    ASSERT_TRUE(second.Update(0));
    const auto expected = glm::quat_cast(glm::mat3(first.GetModelPose()[2]));
    ExpectRotation(second.GetModelPose()[2], expected);
    EXPECT_NEAR(std::abs(glm::dot(expected, rotation)), std::cos(0.2f), 1e-5f);
}

TEST(TwoBoneIKTest, TargetRotationUsesSkeletonSpaceUnderRotatedScaledAncestors)
{
    auto data = MakeIKMesh()->GetData();
    auto bones = data.Skeleton->GetBones();
    bones.insert(bones.begin(), {.Name = "Wrapper", .ReferenceTransform = {
        .Translation = {3, 4, 5},
        .Rotation = glm::angleAxis(0.6f, glm::vec3(0, 0, 1)), .Scale = {2, 2, 2}}});
    for (size_t i = 1; i < bones.size(); ++i) ++bones[i].ParentIndex;
    data.Skeleton = Skeleton::Create("Wrapped", std::move(bones));
    data.ReferencePose.clear();
    SkeletalMeshInstance instance(SkeletalMesh::Create(std::move(data)));
    const auto target = glm::vec3(instance.GetModelPose()[0] * glm::vec4(1, 1, 0, 1));
    const auto pole = glm::vec3(instance.GetModelPose()[0] * glm::vec4(0, 2, 0, 1));
    const auto rotation = glm::angleAxis(0.9f, glm::vec3(0, 1, 0));
    const STwoBoneIKConstraint constraint{3, target, pole, 1, rotation};
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[3][3]), target);
    ExpectRotation(instance.GetModelPose()[3], rotation);
}

TEST(TwoBoneIKTest, RejectsInvalidTargetRotationsAndRetainsSettings)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    const STwoBoneIKConstraint valid{2, {1, 1, 0}, {0, 2, 0}, 1, glm::quat(1, 0, 0, 0)};
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&valid, 1)));
    for (const auto rotation : {glm::quat(0, 0, 0, 0), glm::quat(2, 0, 0, 0),
        glm::quat(std::numeric_limits<float>::infinity(), 0, 0, 0),
        glm::quat(1, 0, std::numeric_limits<float>::quiet_NaN(), 0)})
    {
        auto invalid = valid;
        invalid.TargetRotation = rotation;
        EXPECT_FALSE(instance.SetTwoBoneIKConstraints(std::span(&invalid, 1)));
        EXPECT_EQ(instance.GetTwoBoneIKConstraints()[0].TargetRotation, valid.TargetRotation);
    }
}

TEST(TwoBoneIKTest, ClampedTargetsStillReceiveTheRequestedRotation)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    const auto rotation = glm::angleAxis(-0.9f, glm::vec3(1, 0, 0));
    const STwoBoneIKConstraint constraint{2, {0, 100, 0}, {0, 2, 0}, 1, rotation};
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose()[2][3]), {0, 2, 0});
    ExpectRotation(instance.GetModelPose()[2], rotation);
}

TEST(TwoBoneIKTest, RemovingOrientationRestoresPositionOnlyBehaviorWithoutAccumulation)
{
    SkeletalMeshInstance instance(MakeIKMesh());
    const auto rotation = glm::angleAxis(1.0f, glm::vec3(0, 1, 0));
    const STwoBoneIKConstraint constraint{2, {1, 1, 0}, {0, 2, 0}, 1, rotation};
    ASSERT_TRUE(instance.SetTwoBoneIKConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    const auto pose = instance.GetModelPose();
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetModelPose(), pose);
    ASSERT_TRUE(SetTarget(instance, {1, 1, 0}));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetLocalPose()[2].Rotation, glm::quat(1, 0, 0, 0));
}
