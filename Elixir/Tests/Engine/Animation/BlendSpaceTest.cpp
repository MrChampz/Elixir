#include <Engine/Animation/Pose.h>
#include "AnimationTestNodes.h"
#include "AnimationTestCharacter.h"
#include "AnimationTestPlayback.h"
#include <gtest/gtest.h>
#include "AnimationGraphTestDriver.h"
#include <Engine/Animation/BlendSpace1D.h>
#include <Engine/Animation/BlendSpace2D.h>
#include "../Mesh/SkinningTestData.h"
#include <limits>

using namespace Elixir;

namespace
{
    /** Exercises additional dimensions through the production base API. */
    class TestBlendSpace final : public BlendSpace
    {
      public:
        /**
         * @brief Create a synthetic source with one contribution per axis plus an origin.
         * @param idle Origin clip.
         * @param move Alternate axis clip.
         * @param dimensions Number of numeric axes.
         */
        TestBlendSpace(
            Ref<const AnimationSequence> idle,
            Ref<const AnimationSequence> move,
            size_t dimensions
        )
            : BlendSpace("Test", idle->GetSkeleton(), dimensions), m_Idle(std::move(idle)),
              m_Move(std::move(move))
        {
        }

      private:
        /**
         * @brief Assign axis weights to alternating clips and the remainder to the origin.
         * @param values Validated coordinates in [0, 1].
         * @param output Receives all contributing clips, including the origin.
         */
        void FindSamples(
            std::span<const float> values,
            std::pmr::vector<SSequenceContribution>& output
        ) const override
        {
            float sum = 0;
            for (size_t i = 0; i < values.size(); ++i)
            {
                output.push_back({i % 2 == 0 ? m_Move.get() : m_Idle.get(), values[i]});
                sum += values[i];
            }
            output.push_back({m_Idle.get(), std::max(0.0f, 1.0f - sum)});
        }

        Ref<const AnimationSequence> m_Idle;
        Ref<const AnimationSequence> m_Move;
    };
}

TEST(BlendSpaceTest, GenericControllerAndPlaybackSupportAdditionalDimensionsAndContributions)
{
    const auto mesh = SkinningTests::MakeMesh();
    ASSERT_TRUE(mesh);
    const auto idle = AnimationSequence::Create("Idle", mesh->GetSkeleton(), 1, {});
    const auto move = AnimationSequence::Create(
        "Move", mesh->GetSkeleton(), 2, {{.BoneIndex = 0, .Translations = {{0, {10, 0, 0}}}}}
    );
    for (const size_t dimensions : {1, 2, 3, 5, 17})
    {
        SCOPED_TRACE(dimensions);
        Ref<const BlendSpace> space = CreateRef<TestBlendSpace>(idle, move, dimensions);
        EXPECT_EQ(space->GetDimensionCount(), dimensions);
        std::vector<float> values(dimensions, 0.5f / float(dimensions));
        const auto movingWeight = float((dimensions + 1) / 2) * values[0];
        EXPECT_NEAR(space->GetDuration(values), 1 + movingWeight, 1e-6f);
        SLocalPose pose;
        ASSERT_TRUE(space->Sample(values, 0.25f, pose));
        EXPECT_NEAR(pose.LocalTransforms[0].Translation.x, 10 * movingWeight, 1e-5f);
        SAnimationGraphTestInfo info{
            .Name = "Generic",
            .Layers = {{ .Node = MakeNode(space) }},
        };
        for (size_t axis = 0; axis < dimensions; ++axis)
        {
            const auto name = std::to_string(axis);
            info.Parameters.push_back({name, values[axis]});
            info.Layers[0].InputParameters.push_back(name);
        }
        const auto controller = CreateTestGraph(info);
        ASSERT_TRUE(controller);
        const auto firstHandle = AnimationTestCharacter::Create(mesh);
        ASSERT_TRUE(firstHandle);
        auto& first = *firstHandle;
        const auto secondHandle = AnimationTestCharacter::Create(mesh);
        ASSERT_TRUE(secondHandle);
        auto& second = *secondHandle;
        AnimationGraphTestDriver instance(controller, first);
        ASSERT_TRUE(instance.Update((1 + movingWeight) * 0.25f));
        EXPECT_NEAR(first.GetNormalizedTime(), 0.25f, 1e-6f);
        EXPECT_NEAR(first.GetLocalPose().LocalTransforms[0].Translation.x, 10 * movingWeight, 1e-5f);
        ASSERT_TRUE(instance.SetFloat("0", 0));
        ASSERT_TRUE(instance.Update(0));
        EXPECT_NEAR(first.GetNormalizedTime(), 0.25f, 1e-6f);
        ASSERT_TRUE(PlayWithInputs(second, MakeNode(space), values));
        values.assign(dimensions, 0);   // Playback must own its coordinates.
        ASSERT_TRUE(second.Update(0));
        EXPECT_NEAR(second.GetLocalPose().LocalTransforms[0].Translation.x, 10 * movingWeight, 1e-5f);
        ASSERT_TRUE(second.SetInputs(values));
        ASSERT_TRUE(second.Update(0));
        EXPECT_FLOAT_EQ(second.GetLocalPose().LocalTransforms[0].Translation.x, 0);
        EXPECT_NEAR(
            first.GetLocalPose().LocalTransforms[0].Translation.x,
            10 * (movingWeight - 0.5f / float(dimensions)), 1e-5f
        );
        info.Layers[0].Looping = false;
        const auto nonlooping = CreateTestGraph(info);
        ASSERT_TRUE(nonlooping);
        const auto completedMeshHandle = AnimationTestCharacter::Create(mesh);
        ASSERT_TRUE(completedMeshHandle);
        auto& completedMesh = *completedMeshHandle;
        AnimationGraphTestDriver completed(nonlooping, completedMesh);
        ASSERT_TRUE(completed.Update((1 + movingWeight) * 2));
        EXPECT_FLOAT_EQ(completedMesh.GetNormalizedTime(), 1);
        ASSERT_TRUE(completed.Update(1));
        EXPECT_FLOAT_EQ(completedMesh.GetNormalizedTime(), 1);
        completed.Reset();
        ASSERT_TRUE(completed.Update(0));
        EXPECT_FLOAT_EQ(completedMesh.GetNormalizedTime(), 0);
        EXPECT_TRUE(completed.GetCurrentState().empty());
        info.Layers[0].Node = MakeNode(idle);
        EXPECT_FALSE(CreateTestGraph(info));
        info.Layers[0].Node.reset();
        info.Parameters[0].DefaultValue = true;
        EXPECT_FALSE(CreateTestGraph(info));
        info.Parameters[0].DefaultValue = 0.5f / float(dimensions);
        info.Layers[0].InputParameters.pop_back();
        EXPECT_FALSE(CreateTestGraph(info));
        info.Layers[0].InputParameters.push_back("Missing");
        EXPECT_FALSE(CreateTestGraph(info));
    }
}

TEST(BlendSpaceTest, CommonInterfaceRejectsWrongAxisCountsAndPreservesPlaybackOnFailure)
{
    const auto mesh = SkinningTests::MakeMesh();
    ASSERT_TRUE(mesh);
    const auto clip = AnimationSequence::Create("Clip", mesh->GetSkeleton(), 2, {});
    const std::array<Ref<const BlendSpace>, 2> spaces{
        BlendSpace1D::Create("Line", {{0, clip}}),
        BlendSpace2D::Create(
            "Plane", {
                         {{0, 0}, clip},
                         {{1, 0}, clip},
                         {{0, 1}, clip},
                     }
        ),
    };
    for (const auto& space : spaces)
    {
        ASSERT_TRUE(space);
        std::vector<float> values(space->GetDimensionCount(), 0.25f);
        const auto instanceHandle = AnimationTestCharacter::Create(mesh);
        ASSERT_TRUE(instanceHandle);
        auto& instance = *instanceHandle;
        ASSERT_TRUE(PlayWithInputs(instance, MakeNode(space), values));
        ASSERT_TRUE(instance.Update(0.5f));
        const auto phase = instance.GetNormalizedTime();
        auto pose = instance.GetLocalPose();
        pose.LocalTransforms[0].Translation.x = 99;
        EXPECT_FALSE(space->Sample({}, 0, pose));
        EXPECT_FLOAT_EQ(pose.LocalTransforms[0].Translation.x, 99);
        EXPECT_FALSE(PlayWithInputs(instance, MakeNode(space), {}));
        EXPECT_FALSE(instance.SetInputs({}));
        values.push_back(0);
        EXPECT_FALSE(space->HasValidCoordinates(values));
        EXPECT_FALSE(instance.SetInputs(values));
        values.pop_back();
        values.back() = std::numeric_limits<float>::infinity();
        EXPECT_FALSE(PlayWithInputs(instance, MakeNode(space), values));
        EXPECT_FALSE(instance.SetInputs(values));
        EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), phase);
    }
}
