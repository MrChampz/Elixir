#include "TwoBoneIKTestUtils.h"

#include <limits>

using namespace Elixir;
using namespace Elixir::Tests::IK;
using TwoBoneIKRotationTest = TwoBoneIKFixture;

TEST_F(TwoBoneIKRotationTest, TargetRotationAlignsTheEndWithoutMovingItsPosition)
{
    const auto rotation = glm::angleAxis(0.8f, glm::normalize(glm::vec3(1, 2, 3)));
    const STwoBoneIKConstraint constraint{2, {1, 1, 0}, {0, 2, 0}, 1, rotation};
    ASSERT_TRUE(Solver->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(Playback->Update(0));
    const auto& model = Playback->GetModelPose().Matrices;
    ExpectPosition(glm::vec3(model[2][3]), {1, 1, 0});
    ExpectRotation(model[2], rotation);
    const auto* grip = Mesh->GetSkeleton()->GetSocket("Grip");
    ASSERT_TRUE(grip);
    const auto attachment = grip->GetTransform(Playback->GetModelPose());
    ASSERT_TRUE(attachment);
    ExpectRotation(*attachment, rotation);
    ExpectPosition(
        glm::vec3(model[3][3]),
        glm::vec3(1, 1, 0) + rotation * glm::vec3(0.2f, 0, 0)
    );
}

TEST_F(TwoBoneIKRotationTest, AbsentRotationPreservesAnimatedEndLocalRotation)
{
    auto data = Mesh->GetData();
    const auto rotation = glm::angleAxis(0.6f, glm::vec3(1, 0, 0));
    data.ReferencePose.LocalTransforms[2].Rotation = rotation;
    ASSERT_TRUE(Initialize(SkeletalMesh::Create(std::move(data))));
    ASSERT_TRUE(SetTarget(*Solver, {1, 1, 0}));
    ASSERT_TRUE(Playback->Update(0));
    EXPECT_EQ(Playback->GetLocalPose().LocalTransforms[2].Rotation, rotation);
}

TEST_F(TwoBoneIKRotationTest, PartialWeightBlendsFromThePositionCorrectedOrientation)
{
    const auto positionHandle = MakeIKAnimation(Mesh);
    ASSERT_TRUE(positionHandle);
    auto& positionOnly = *positionHandle;
    const auto positionIK = AttachIK(positionOnly);
    ASSERT_TRUE(positionIK);
    const auto rotation = glm::angleAxis(1.4f, glm::vec3(0, 0, 1));
    ASSERT_TRUE(SetTarget(*positionIK, {1, 1, 0}, {0, 2, 0}, 0.5f));
    ASSERT_TRUE(positionOnly.Update(0));
    const auto initialRotation = glm::quat_cast(
        glm::mat3(positionOnly.GetModelPose().Matrices[2])
    );
    const STwoBoneIKConstraint constraint{2, {1, 1, 0}, {0, 2, 0}, 0.5f, rotation};
    ASSERT_TRUE(Solver->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(Playback->Update(0));
    ExpectRotation(
        Playback->GetModelPose().Matrices[2],
        glm::slerp(initialRotation, rotation, 0.5f)
    );
    ExpectPosition(
        glm::vec3(Playback->GetModelPose().Matrices[2][3]),
        glm::vec3(positionOnly.GetModelPose().Matrices[2][3])
    );
}

TEST_F(TwoBoneIKRotationTest, ZeroWeightPreservesTheEntirePoseWithATargetRotation)
{
    const auto pose = Playback->GetModelPose().Matrices;
    const STwoBoneIKConstraint constraint{2, {1, 1, 0}, {0, 2, 0}, 0,
        glm::angleAxis(1.2f, glm::vec3(0, 1, 0))};
    ASSERT_TRUE(Solver->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(Playback->Update(0));
    EXPECT_EQ(Playback->GetModelPose().Matrices, pose);
}

TEST_F(TwoBoneIKRotationTest, OppositeQuaternionSignsGiveTheSameShortestPath)
{
    const auto secondHandle = MakeIKAnimation(Mesh);
    ASSERT_TRUE(secondHandle);
    auto& second = *secondHandle;
    const auto secondIK = AttachIK(second);
    ASSERT_TRUE(secondIK);
    const auto rotation = glm::angleAxis(0.8f, glm::normalize(glm::vec3(1, 2, 3)));
    const STwoBoneIKConstraint positive{2, {1, 1, 0}, {0, 2, 0}, 0.5f, rotation};
    const STwoBoneIKConstraint negative{2, {1, 1, 0}, {0, 2, 0}, 0.5f, -rotation};
    ASSERT_TRUE(Solver->SetConstraints(std::span(&positive, 1)));
    ASSERT_TRUE(secondIK->SetConstraints(std::span(&negative, 1)));
    ASSERT_TRUE(Playback->Update(0));
    ASSERT_TRUE(second.Update(0));
    const auto expected = glm::quat_cast(glm::mat3(Playback->GetModelPose().Matrices[2]));
    ExpectRotation(second.GetModelPose().Matrices[2], expected);
    EXPECT_NEAR(std::abs(glm::dot(expected, rotation)), std::cos(0.2f), 1e-5f);
}

TEST_F(TwoBoneIKRotationTest, TargetRotationUsesSkeletonSpaceUnderRotatedScaledAncestors)
{
    auto data = Mesh->GetData();
    auto bones = data.Skeleton->GetBones();
    bones.insert(
        bones.begin(),
        {
            .Name = "Wrapper",
            .ReferenceTransform = {
                .Translation = {3, 4, 5},
                .Rotation = glm::angleAxis(0.6f, glm::vec3(0, 0, 1)),
                .Scale = {2, 2, 2}
            }
        }
    );
    for (size_t i = 1; i < bones.size(); ++i) ++bones[i].ParentIndex;
    data.Skeleton = Skeleton::Create("Wrapped", std::move(bones));
    data.ReferencePose.LocalTransforms.clear();
    ASSERT_TRUE(Initialize(SkeletalMesh::Create(std::move(data))));
    const auto& model = Playback->GetModelPose().Matrices;
    const auto target = glm::vec3(model[0] * glm::vec4(1, 1, 0, 1));
    const auto pole = glm::vec3(model[0] * glm::vec4(0, 2, 0, 1));
    const auto rotation = glm::angleAxis(0.9f, glm::vec3(0, 1, 0));
    const STwoBoneIKConstraint constraint{3, target, pole, 1, rotation};
    ASSERT_TRUE(Solver->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(Playback->Update(0));
    ExpectPosition(glm::vec3(Playback->GetModelPose().Matrices[3][3]), target);
    ExpectRotation(Playback->GetModelPose().Matrices[3], rotation);
}

TEST_F(TwoBoneIKRotationTest, RejectsInvalidTargetRotationsAndRetainsSettings)
{
    const STwoBoneIKConstraint valid{2, {1, 1, 0}, {0, 2, 0}, 1, glm::quat(1, 0, 0, 0)};
    ASSERT_TRUE(Solver->SetConstraints(std::span(&valid, 1)));
    for (const auto rotation : {glm::quat(0, 0, 0, 0), glm::quat(2, 0, 0, 0),
        glm::quat(std::numeric_limits<float>::infinity(), 0, 0, 0),
        glm::quat(1, 0, std::numeric_limits<float>::quiet_NaN(), 0)})
    {
        auto invalid = valid;
        invalid.TargetRotation = rotation;
        EXPECT_FALSE(Solver->SetConstraints(std::span(&invalid, 1)));
        EXPECT_EQ(Solver->GetConstraints()[0].TargetRotation, valid.TargetRotation);
    }
}

TEST_F(TwoBoneIKRotationTest, ClampedTargetsStillReceiveTheRequestedRotation)
{
    const auto rotation = glm::angleAxis(-0.9f, glm::vec3(1, 0, 0));
    const STwoBoneIKConstraint constraint{2, {0, 100, 0}, {0, 2, 0}, 1, rotation};
    ASSERT_TRUE(Solver->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(Playback->Update(0));
    ExpectPosition(glm::vec3(Playback->GetModelPose().Matrices[2][3]), {0, 2, 0});
    ExpectRotation(Playback->GetModelPose().Matrices[2], rotation);
}

TEST_F(
    TwoBoneIKRotationTest,
    RemovingOrientationRestoresPositionOnlyBehaviorWithoutAccumulation
)
{
    const auto rotation = glm::angleAxis(1.0f, glm::vec3(0, 1, 0));
    const STwoBoneIKConstraint constraint{2, {1, 1, 0}, {0, 2, 0}, 1, rotation};
    ASSERT_TRUE(Solver->SetConstraints(std::span(&constraint, 1)));
    ASSERT_TRUE(Playback->Update(0));
    const auto pose = Playback->GetModelPose().Matrices;
    ASSERT_TRUE(Playback->Update(0));
    EXPECT_EQ(Playback->GetModelPose().Matrices, pose);
    ASSERT_TRUE(SetTarget(*Solver, {1, 1, 0}));
    ASSERT_TRUE(Playback->Update(0));
    EXPECT_EQ(Playback->GetLocalPose().LocalTransforms[2].Rotation, glm::quat(1, 0, 0, 0));
}
