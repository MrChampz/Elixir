#include <Engine/Animation/Pose.h>

#include <gtest/gtest.h>

#include <limits>

using namespace Elixir;

TEST(LocalPoseTest, RequiresOneTransformPerExpectedBone)
{
    const SLocalPose empty;
    EXPECT_TRUE(empty.IsValid(0));
    EXPECT_FALSE(empty.IsValid(1));

    const SLocalPose pose{{SBoneTransform{}, SBoneTransform{}}};
    EXPECT_TRUE(pose.IsValid(2));
    EXPECT_FALSE(pose.IsValid(1));
    EXPECT_FALSE(pose.IsValid(3));
}

TEST(LocalPoseTest, RejectsInvalidTransformsThroughoutThePose)
{
    SLocalPose pose{{SBoneTransform{}, SBoneTransform{}}};
    pose.LocalTransforms[1].Translation.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(pose.IsValid(2));

    pose.LocalTransforms[1] = {};
    pose.LocalTransforms[0].Rotation = glm::quat(2, 0, 0, 0);
    EXPECT_FALSE(pose.IsValid(2));

    pose.LocalTransforms[0] = {};
    pose.LocalTransforms[1].Scale.y = 0;
    EXPECT_FALSE(pose.IsValid(2));

    pose.LocalTransforms[1] = {};
    EXPECT_TRUE(pose.IsValid(2));
}

TEST(ModelPoseTest, RequiresOneMatrixPerExpectedBone)
{
    const SModelPose empty;
    EXPECT_TRUE(empty.IsValid(0));
    EXPECT_FALSE(empty.IsValid(1));

    const SModelPose pose{{glm::mat4(1), glm::mat4(1)}};
    EXPECT_TRUE(pose.IsValid(2));
    EXPECT_FALSE(pose.IsValid(1));
    EXPECT_FALSE(pose.IsValid(3));
}

TEST(ModelPoseTest, RejectsNonfiniteComponentsInEveryBoneMatrix)
{
    SModelPose pose{{glm::mat4(1), glm::mat4(1)}};
    for (auto& matrix : pose.Matrices)
    {
        for (glm::length_t c = 0; c < 4; ++c)
        {
            for (glm::length_t r = 0; r < 4; ++r)
            {
                const auto original = matrix[c][r];
                matrix[c][r] = std::numeric_limits<float>::quiet_NaN();
                EXPECT_FALSE(pose.IsValid(2));
                matrix[c][r] = std::numeric_limits<float>::infinity();
                EXPECT_FALSE(pose.IsValid(2));
                matrix[c][r] = original;
            }
        }
    }
    EXPECT_TRUE(pose.IsValid(2));
}
