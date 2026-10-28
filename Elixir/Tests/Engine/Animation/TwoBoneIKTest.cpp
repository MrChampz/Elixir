#include <gtest/gtest.h>
#include "../Mesh/SkinningTestData.h"
#include <limits>
#include <Engine/Animation/Animation.h>
#include <Engine/Animation/TwoBoneIK.h>
#include <Engine/Animation/SequenceNode.h>

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
        data.ReferencePose.LocalTransforms.clear();
        return SkeletalMesh::Create(std::move(data));
    }

    /**
     * @brief Create independent animation state for a test mesh.
     * @param mesh Mesh supplying the fixed skeleton and reference pose.
     */
    Ref<Animation> MakeIKAnimation(const Ref<const SkeletalMesh>& mesh)
    {
        if (!mesh) return nullptr;
        return Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
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
     * @brief Attach a separately configured solver to an animation's generic pipeline.
     * @param animation Animation that retains the modifier during pose evaluation.
     */
    Ref<TwoBoneIK> AttachIK(Animation& animation)
    {
        const auto solver = TwoBoneIK::Create(animation.GetSkeleton());
        if (!solver) return nullptr;
        const std::array<Ref<const PoseModifier>, 1> modifiers{solver};
        return animation.SetPoseModifiers(modifiers) ? solver : nullptr;
    }

    /**
     * @brief Configure one limb target on an external solver.
     * @param solver Modifier to configure.
     * @param target Skeleton-space destination.
     * @param pole Skeleton-space bend point.
     * @param weight Rotation blend weight.
     */
    bool SetTarget(
        TwoBoneIK& solver,
        glm::vec3 target,
        glm::vec3 pole = {0, 2, 0},
        float weight = 1
    )
    {
        const STwoBoneIKConstraint constraint{2, target, pole, weight};
        return solver.SetConstraints(std::span(&constraint, 1));
    }
}

TEST(TwoBoneIKTest, ReachesTargetPreservesLengthsAndUpdatesSocketsAndDescendants)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}));
    EXPECT_FLOAT_EQ(instance.GetModelPose().Matrices[2][3].x, 2);
    ASSERT_TRUE(instance.Update(0));
    const auto& model = instance.GetModelPose().Matrices;
    ExpectPosition(glm::vec3(model[2][3]), {1, 1, 0});
    ExpectPosition(glm::vec3(model[1][3]), {0, 1, 0});
    EXPECT_NEAR(glm::length(glm::vec3(model[1][3] - model[0][3])), 1, 1e-5f);
    EXPECT_NEAR(glm::length(glm::vec3(model[2][3] - model[1][3])), 1, 1e-5f);
    const auto* grip = mesh->GetSkeleton()->GetSocket("Grip");
    ASSERT_TRUE(grip);
    const auto attachment = grip->GetTransform(instance.GetModelPose());
    ASSERT_TRUE(attachment);
    ExpectPosition(glm::vec3((*attachment)[3]), {1, 1, 0});
    ExpectPosition(glm::vec3(model[3][3]), {1.2f, 1, 0});
    ExpectPosition(glm::vec3(model[4][3]), {-1, 0, 0});
    for (size_t i = 1; i < instance.GetLocalPose().LocalTransforms.size(); ++i)
        ExpectPosition(instance.GetLocalPose().LocalTransforms[i].Translation,
            mesh->GetData().ReferencePose.LocalTransforms[i].Translation);
}

TEST(TwoBoneIKTest, PoleSelectsOppositeBend)
{
    const auto mesh = MakeIKMesh();
    const auto firstHandle = MakeIKAnimation(mesh);
    const auto secondHandle = MakeIKAnimation(mesh);
    ASSERT_TRUE(firstHandle);
    ASSERT_TRUE(secondHandle);
    auto& first = *firstHandle;
    auto& second = *secondHandle;
    const auto firstIK = AttachIK(first);
    const auto secondIK = AttachIK(second);
    ASSERT_TRUE(firstIK);
    ASSERT_TRUE(secondIK);
    ASSERT_TRUE(SetTarget(*firstIK, {1, 1, 0}, {0, 2, 0}));
    ASSERT_TRUE(SetTarget(*secondIK, {1, 1, 0}, {2, 0, 0}));
    ASSERT_TRUE(first.Update(0));
    ASSERT_TRUE(second.Update(0));
    ExpectPosition(glm::vec3(first.GetModelPose().Matrices[1][3]), {0, 1, 0});
    ExpectPosition(glm::vec3(second.GetModelPose().Matrices[1][3]), {1, 0, 0});
    ExpectPosition(glm::vec3(second.GetModelPose().Matrices[2][3]), {1, 1, 0});
}

TEST(TwoBoneIKTest, ZeroWeightIsExactAndPartialWeightBlendsLocalRotations)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    const auto initial = instance.GetModelPose().Matrices;
    ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}, {0, 2, 0}, 0));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetModelPose().Matrices, initial);
    ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}, {0, 2, 0}, 0.5f));
    ASSERT_TRUE(instance.Update(0));
    const auto angle = glm::angle(instance.GetLocalPose().LocalTransforms[0].Rotation);
    EXPECT_NEAR(angle, glm::radians(45.0f), 1e-5f);
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[2][3]),
        {1 + std::sqrt(0.5f), std::sqrt(0.5f), 0});
}

TEST(TwoBoneIKTest, FarTargetsClampWithoutStretching)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    ASSERT_TRUE(SetTarget(*ik, {0, 100, 0}));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[2][3]), {0, 2, 0});
}

TEST(TwoBoneIKTest, NearTargetsClampToUnequalSegmentReach)
{
    auto data = MakeIKMesh()->GetData();
    data.ReferencePose.LocalTransforms[1].Translation.x = 2;
    const auto mesh = SkeletalMesh::Create(std::move(data));
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    ASSERT_TRUE(SetTarget(*ik, {0.1f, 0, 0}));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[2][3]), {1, 0, 0});
}

TEST(TwoBoneIKTest, CoincidentTargetAndDegeneratePolesRemainFinite)
{
    for (const auto target : {glm::vec3(0), glm::vec3(-1, 0, 0), glm::vec3(2, 0, 0)})
    {
        const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
        ASSERT_TRUE(SetTarget(*ik, target, {0, 0, 0}));
        ASSERT_TRUE(instance.Update(0));
        ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[2][3]), target);
        for (const auto& transform : instance.GetLocalPose().LocalTransforms)
            EXPECT_TRUE(transform.IsValid());
    }
}

TEST(TwoBoneIKTest, SupportsAThreeDimensionalLegTarget)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    ASSERT_TRUE(SetTarget(*ik, {0, -1, 1}, {1, -1, 0}));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[2][3]), {0, -1, 1});
    EXPECT_GT(instance.GetModelPose().Matrices[1][3].x, 0);
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
    data.ReferencePose.LocalTransforms.clear();
    const auto mesh = SkeletalMesh::Create(std::move(data));
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    const auto target = glm::vec3(instance.GetModelPose().Matrices[0] * glm::vec4(1, 1, 0, 1));
    const auto pole = glm::vec3(instance.GetModelPose().Matrices[0] * glm::vec4(0, 2, 0, 1));
    const STwoBoneIKConstraint constraint{3, target, pole};
    ASSERT_TRUE(ik->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[3][3]), target);
}

TEST(TwoBoneIKTest, RejectsInvalidSettingsAndPreservesConfiguration)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}));
    for (const auto weight : {-1.0f, 1.1f, std::numeric_limits<float>::quiet_NaN()})
        EXPECT_FALSE(SetTarget(*ik, {1, 0, 0}, {0, 1, 0}, weight));
    EXPECT_FALSE(SetTarget(*ik, {std::numeric_limits<float>::infinity(), 0, 0}));
    EXPECT_FALSE(SetTarget(*ik, {}, {0, std::numeric_limits<float>::quiet_NaN(), 0}));
    for (const auto bone : {0u, 1u, 20u})
    {
        const STwoBoneIKConstraint constraint{.EndBoneIndex = bone};
        EXPECT_FALSE(ik->SetConstraints(std::span(&constraint, 1)));
    }
    ASSERT_EQ(ik->GetConstraints().size(), 1);
    ExpectPosition(ik->GetConstraints()[0].TargetPosition, {1, 1, 0});
    EXPECT_TRUE(ik->SetConstraints(ik->GetConstraints()));
    EXPECT_FALSE(TwoBoneIK::Create(nullptr));
}

TEST(TwoBoneIKTest, FailedEvaluationPreservesPlaybackPoseAndNotifyBatch)
{
    const auto mesh = MakeIKMesh();
    const auto sequence = Sequence::Create("Scale", mesh->GetSkeleton(), 1, {{
        .BoneIndex = 1,
        .Scales = {{0, {1, 1, 1}}, {1, {2, 1, 1}}}
    }}, {}, {{"Event", 0.1f}});
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    ASSERT_TRUE(instance.Play(SequenceNode::Create(sequence)));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}));
    const auto pose = instance.GetModelPose().Matrices;
    EXPECT_FALSE(instance.Update(0.2f));
    EXPECT_EQ(instance.GetModelPose().Matrices, pose);
    EXPECT_FLOAT_EQ(instance.GetTime(), 0.1f);
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    ASSERT_TRUE(ik->SetConstraints({}));
    EXPECT_TRUE(instance.Update(0.2f));
}

TEST(TwoBoneIKTest, RejectsZeroLengthsAndNegativeScalesOnlyWhenActive)
{
    for (const auto scale : {glm::vec3(-1), glm::vec3(2, 1, 1)})
    {
        auto data = MakeIKMesh()->GetData();
        data.ReferencePose.LocalTransforms[0].Scale = scale;
        const auto mesh = SkeletalMesh::Create(std::move(data));
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
        ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}));
        EXPECT_FALSE(instance.Update(0));
        ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}, {0, 1, 0}, 0));
        EXPECT_TRUE(instance.Update(0));
    }
    auto data = MakeIKMesh()->GetData();
    data.ReferencePose.LocalTransforms[1].Translation = {};
    const auto mesh = SkeletalMesh::Create(std::move(data));
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}));
    EXPECT_FALSE(instance.Update(0));
}

TEST(TwoBoneIKTest, ReadsAfterAdditiveLayersAndMontagesWithoutChangingTheirPlayback)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    const auto sequence = Sequence::Create("Lift", mesh->GetSkeleton(), 1, {{
        .BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {0, 1, 0}}}
    }});
    const auto mask = BoneMask::FromBone("All", mesh->GetSkeleton(), "Root");
    ASSERT_TRUE(instance.AddLayer("Action", mask));
    auto* layer = instance.GetLayer("Action");
    ASSERT_TRUE(layer->SetBlendMode(EAnimationLayerBlendMode::Additive));
    const auto montage = AnimationMontage::Create({
        .Name = "Lift", .Sequence = sequence, .BlendIn = 0, .BlendOut = 0
    });
    ASSERT_TRUE(layer->GetPlayer().PlayMontage(montage));
    ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}));
    ASSERT_TRUE(instance.Update(0.5f));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[2][3]), {1, 1, 0});
    EXPECT_FLOAT_EQ(layer->GetPlayer().GetMontageTime(), 0.5f);
    EXPECT_FLOAT_EQ(instance.GetModelPose().Matrices[0][3].y, 0.5f);
}

TEST(TwoBoneIKTest, UpdatingPausedTargetsDoesNotAccumulateCorrections)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}, {0, 2, 0}, 0.5f));
    instance.Pause();
    ASSERT_TRUE(instance.Update(0));
    const auto pose = instance.GetModelPose().Matrices;
    ASSERT_TRUE(instance.Update(1));
    EXPECT_EQ(instance.GetModelPose().Matrices, pose);
    ASSERT_TRUE(SetTarget(*ik, {0, 1, 0}));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[2][3]), {0, 1, 0});
    ASSERT_TRUE(ik->SetConstraints({}));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[2][3]), {2, 0, 0});
}

TEST(TwoBoneIKTest, ManualPoseIsSolvedAndStopRestoresReferencePose)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}));
    ASSERT_TRUE(instance.SetLocalPose(instance.GetLocalPose()));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[2][3]), {1, 1, 0});
    instance.Stop();
    EXPECT_TRUE(instance.GetPoseModifiers().empty());
    EXPECT_EQ(ik->GetConstraints().size(), 1u);
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[2][3]), {2, 0, 0});
}

TEST(TwoBoneIKTest, LaterConstraintsUseThePoseProducedByEarlierOnes)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    const std::array<STwoBoneIKConstraint, 2> constraints = {{
        {2, {1, 1, 0}, {0, 2, 0}}, {2, {1, -1, 0}, {0, -2, 0}}
    }};
    ASSERT_TRUE(ik->SetConstraints(constraints));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[2][3]), {1, -1, 0});
}

TEST(TwoBoneIKTest, ProtectsTheExtractedRootMotionFrame)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    ASSERT_TRUE(instance.SetRootMotionEnabled(true));
    ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}));
    EXPECT_FALSE(instance.Update(0));
    ASSERT_TRUE(instance.SetRootMotionEnabled(false));
    ASSERT_TRUE(instance.Update(0));
    const auto pose = instance.GetModelPose().Matrices;
    EXPECT_FALSE(instance.SetRootMotionEnabled(true));
    EXPECT_FALSE(instance.IsRootMotionEnabled());
    EXPECT_EQ(instance.GetModelPose().Matrices, pose);
}

TEST(TwoBoneIKTest, ProcessesStandalonePosesAndRejectsIncompatibleContexts)
{
    const auto mesh = MakeIKMesh();
    const auto& skeleton = mesh->GetSkeleton();
    const auto solver = TwoBoneIK::Create(skeleton);
    ASSERT_TRUE(solver);
    ASSERT_TRUE(SetTarget(*solver, {1, 1, 0}));
    auto pose = mesh->GetData().ReferencePose;
    ASSERT_TRUE(solver->Apply({*skeleton, std::nullopt}, pose));
    SModelPose model;
    ASSERT_TRUE(skeleton->BuildModelPose(pose, model));
    ExpectPosition(glm::vec3(model.Matrices[2][3]), {1, 1, 0});
    auto bones = skeleton->GetBones();
    bones[0].Name = "DifferentRoot";
    const auto incompatible = Skeleton::Create("DifferentRig", std::move(bones));
    ASSERT_TRUE(incompatible);
    EXPECT_FALSE(solver->Apply({*incompatible, std::nullopt}, pose));
    EXPECT_FALSE(solver->Apply({*skeleton, UINT32_MAX}, pose));
    SLocalPose invalid;
    EXPECT_FALSE(solver->Apply({*skeleton, std::nullopt}, invalid));
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
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    const auto rotation = glm::angleAxis(0.8f, glm::normalize(glm::vec3(1, 2, 3)));
    const STwoBoneIKConstraint constraint{2, {1, 1, 0}, {0, 2, 0}, 1, rotation};
    ASSERT_TRUE(ik->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    const auto& model = instance.GetModelPose().Matrices;
    ExpectPosition(glm::vec3(model[2][3]), {1, 1, 0});
    ExpectRotation(model[2], rotation);
    const auto* grip = mesh->GetSkeleton()->GetSocket("Grip");
    ASSERT_TRUE(grip);
    const auto attachment = grip->GetTransform(instance.GetModelPose());
    ASSERT_TRUE(attachment);
    ExpectRotation(*attachment, rotation);
    ExpectPosition(
        glm::vec3(model[3][3]),
        glm::vec3(1, 1, 0) + rotation * glm::vec3(0.2f, 0, 0)
    );
}

TEST(TwoBoneIKTest, AbsentRotationPreservesAnimatedEndLocalRotation)
{
    auto data = MakeIKMesh()->GetData();
    const auto rotation = glm::angleAxis(0.6f, glm::vec3(1, 0, 0));
    data.ReferencePose.LocalTransforms[2].Rotation = rotation;
    const auto mesh = SkeletalMesh::Create(std::move(data));
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetLocalPose().LocalTransforms[2].Rotation, rotation);
}

TEST(TwoBoneIKTest, PartialWeightBlendsFromThePositionCorrectedOrientation)
{
    const auto mesh = MakeIKMesh();
    const auto positionHandle = MakeIKAnimation(mesh);
    const auto orientedHandle = MakeIKAnimation(mesh);
    ASSERT_TRUE(positionHandle);
    ASSERT_TRUE(orientedHandle);
    auto& positionOnly = *positionHandle;
    auto& oriented = *orientedHandle;
    const auto positionIK = AttachIK(positionOnly);
    const auto orientedIK = AttachIK(oriented);
    ASSERT_TRUE(positionIK);
    ASSERT_TRUE(orientedIK);
    const auto rotation = glm::angleAxis(1.4f, glm::vec3(0, 0, 1));
    ASSERT_TRUE(SetTarget(*positionIK, {1, 1, 0}, {0, 2, 0}, 0.5f));
    ASSERT_TRUE(positionOnly.Update(0));
    const auto initialRotation = glm::quat_cast(
        glm::mat3(positionOnly.GetModelPose().Matrices[2])
    );
    const STwoBoneIKConstraint constraint{2, {1, 1, 0}, {0, 2, 0}, 0.5f, rotation};
    ASSERT_TRUE(orientedIK->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(oriented.Update(0));
    ExpectRotation(
        oriented.GetModelPose().Matrices[2],
        glm::slerp(initialRotation, rotation, 0.5f)
    );
    ExpectPosition(glm::vec3(oriented.GetModelPose().Matrices[2][3]),
        glm::vec3(positionOnly.GetModelPose().Matrices[2][3]));
}

TEST(TwoBoneIKTest, ZeroWeightPreservesTheEntirePoseWithATargetRotation)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    const auto pose = instance.GetModelPose().Matrices;
    const STwoBoneIKConstraint constraint{2, {1, 1, 0}, {0, 2, 0}, 0,
        glm::angleAxis(1.2f, glm::vec3(0, 1, 0))};
    ASSERT_TRUE(ik->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetModelPose().Matrices, pose);
}

TEST(TwoBoneIKTest, OppositeQuaternionSignsGiveTheSameShortestPath)
{
    const auto mesh = MakeIKMesh();
    const auto firstHandle = MakeIKAnimation(mesh);
    const auto secondHandle = MakeIKAnimation(mesh);
    ASSERT_TRUE(firstHandle);
    ASSERT_TRUE(secondHandle);
    auto& first = *firstHandle;
    auto& second = *secondHandle;
    const auto firstIK = AttachIK(first);
    const auto secondIK = AttachIK(second);
    ASSERT_TRUE(firstIK);
    ASSERT_TRUE(secondIK);
    const auto rotation = glm::angleAxis(0.8f, glm::normalize(glm::vec3(1, 2, 3)));
    const STwoBoneIKConstraint positive{2, {1, 1, 0}, {0, 2, 0}, 0.5f, rotation};
    const STwoBoneIKConstraint negative{2, {1, 1, 0}, {0, 2, 0}, 0.5f, -rotation};
    ASSERT_TRUE(firstIK->SetConstraints(std::span(&positive, 1)));
    ASSERT_TRUE(secondIK->SetConstraints(std::span(&negative, 1)));
    ASSERT_TRUE(first.Update(0));
    ASSERT_TRUE(second.Update(0));
    const auto expected = glm::quat_cast(glm::mat3(first.GetModelPose().Matrices[2]));
    ExpectRotation(second.GetModelPose().Matrices[2], expected);
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
    data.ReferencePose.LocalTransforms.clear();
    const auto mesh = SkeletalMesh::Create(std::move(data));
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    const auto target = glm::vec3(instance.GetModelPose().Matrices[0] * glm::vec4(1, 1, 0, 1));
    const auto pole = glm::vec3(instance.GetModelPose().Matrices[0] * glm::vec4(0, 2, 0, 1));
    const auto rotation = glm::angleAxis(0.9f, glm::vec3(0, 1, 0));
    const STwoBoneIKConstraint constraint{3, target, pole, 1, rotation};
    ASSERT_TRUE(ik->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[3][3]), target);
    ExpectRotation(instance.GetModelPose().Matrices[3], rotation);
}

TEST(TwoBoneIKTest, RejectsInvalidTargetRotationsAndRetainsSettings)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    const STwoBoneIKConstraint valid{2, {1, 1, 0}, {0, 2, 0}, 1, glm::quat(1, 0, 0, 0)};
    ASSERT_TRUE(ik->SetConstraints(std::span(&valid, 1)));
    for (const auto rotation : {glm::quat(0, 0, 0, 0), glm::quat(2, 0, 0, 0),
        glm::quat(std::numeric_limits<float>::infinity(), 0, 0, 0),
        glm::quat(1, 0, std::numeric_limits<float>::quiet_NaN(), 0)})
    {
        auto invalid = valid;
        invalid.TargetRotation = rotation;
        EXPECT_FALSE(ik->SetConstraints(std::span(&invalid, 1)));
        EXPECT_EQ(ik->GetConstraints()[0].TargetRotation, valid.TargetRotation);
    }
}

TEST(TwoBoneIKTest, ClampedTargetsStillReceiveTheRequestedRotation)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    const auto rotation = glm::angleAxis(-0.9f, glm::vec3(1, 0, 0));
    const STwoBoneIKConstraint constraint{2, {0, 100, 0}, {0, 2, 0}, 1, rotation};
    ASSERT_TRUE(ik->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    ExpectPosition(glm::vec3(instance.GetModelPose().Matrices[2][3]), {0, 2, 0});
    ExpectRotation(instance.GetModelPose().Matrices[2], rotation);
}

TEST(TwoBoneIKTest, RemovingOrientationRestoresPositionOnlyBehaviorWithoutAccumulation)
{
    const auto mesh = MakeIKMesh();
    const auto animation = MakeIKAnimation(mesh);
    ASSERT_TRUE(animation);
    auto& instance = *animation;
    const auto ik = AttachIK(instance);
    ASSERT_TRUE(ik);
    const auto rotation = glm::angleAxis(1.0f, glm::vec3(0, 1, 0));
    const STwoBoneIKConstraint constraint{2, {1, 1, 0}, {0, 2, 0}, 1, rotation};
    ASSERT_TRUE(ik->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(instance.Update(0));
    const auto pose = instance.GetModelPose().Matrices;
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetModelPose().Matrices, pose);
    ASSERT_TRUE(SetTarget(*ik, {1, 1, 0}));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetLocalPose().LocalTransforms[2].Rotation, glm::quat(1, 0, 0, 0));
}
