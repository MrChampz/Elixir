#include <gtest/gtest.h>

#include "../Mesh/SkinningTestData.h"
#include <Engine/Animation/Animation.h>
#include <Engine/Animation/SequenceNode.h>

using namespace Elixir;

namespace
{
    /** Changes one translation to test ordered corrections and candidate rejection. */
    class TranslationModifier final : public PoseModifier
    {
    public:
        /**
         * @brief Configure a deterministic translation correction.
         * @param bone Bone whose local translation changes.
         * @param offset Translation added after multiplication.
         * @param multiplier Scale applied to the original translation.
         * @param succeeds Whether to accept the modified candidate.
         */
        TranslationModifier(
            uint32_t bone,
            glm::vec3 offset,
            float multiplier = 1,
            bool succeeds = true
        ) : m_Bone(bone), m_Offset(offset), m_Multiplier(multiplier), m_Succeeds(succeeds) {}

        /**
         * @brief Correct one local translation without changing modifier state.
         * @param context Rig used to check the expected number of bones.
         * @param pose Candidate local pose modified in place.
         */
        bool Apply(const SPoseModifierContext& context, SLocalPose& pose) const override
        {
            if (!pose.IsValid(context.Rig.GetBones().size()) ||
                m_Bone >= pose.LocalTransforms.size())
                return false;
            auto& translation = pose.LocalTransforms[m_Bone].Translation;
            translation = translation * m_Multiplier + m_Offset;
            return m_Succeeds;
        }

    private:
        uint32_t m_Bone;
        glm::vec3 m_Offset;
        float m_Multiplier;
        bool m_Succeeds;
    };

    /** Produces an invalid pose while claiming success to test publication validation. */
    class InvalidPoseModifier final : public PoseModifier
    {
    public:
        /**
         * @brief Remove a transform so the resulting candidate cannot be published.
         * @param context Rig describing the expected complete pose.
         * @param pose Candidate pose intentionally made incomplete.
         */
        bool Apply(const SPoseModifierContext& context, SLocalPose& pose) const override
        {
            pose.LocalTransforms.clear();
            return true;
        }
    };
}

TEST(PoseModifierTest, AppliesOrderedCorrectionsAndRebuildsModelMatrices)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto animation = Animation::Create(
        mesh->GetSkeleton(),
        mesh->GetData().ReferencePose
    );
    ASSERT_TRUE(animation);
    const auto translate = CreateRef<TranslationModifier>(1, glm::vec3(1, 0, 0));
    const auto scale = CreateRef<TranslationModifier>(1, glm::vec3(0), 2);
    const std::array<Ref<const PoseModifier>, 2> modifiers{translate, scale};
    ASSERT_TRUE(animation->SetPoseModifiers(modifiers));
    ASSERT_TRUE(animation->Update(0));
    EXPECT_FLOAT_EQ(animation->GetLocalPose().LocalTransforms[1].Translation.x, 2);
    EXPECT_FLOAT_EQ(animation->GetModelPose().Matrices[1][3].x, 2);
    const std::array<Ref<const PoseModifier>, 2> reversed{scale, translate};
    ASSERT_TRUE(animation->SetPoseModifiers(reversed));
    ASSERT_TRUE(animation->Update(0));
    EXPECT_FLOAT_EQ(animation->GetModelPose().Matrices[1][3].x, 1);
}

TEST(PoseModifierTest, RejectsNullEntriesWithoutReplacingThePipeline)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto animation = Animation::Create(
        mesh->GetSkeleton(),
        mesh->GetData().ReferencePose
    );
    ASSERT_TRUE(animation);
    const auto translate = CreateRef<TranslationModifier>(1, glm::vec3(1, 0, 0));
    const std::array<Ref<const PoseModifier>, 1> valid{translate};
    ASSERT_TRUE(animation->SetPoseModifiers(valid));
    const std::array<Ref<const PoseModifier>, 2> invalid{translate, nullptr};
    EXPECT_FALSE(animation->SetPoseModifiers(invalid));
    ASSERT_EQ(animation->GetPoseModifiers().size(), 1u);
    EXPECT_EQ(animation->GetPoseModifiers()[0], translate);
    EXPECT_TRUE(animation->SetPoseModifiers(animation->GetPoseModifiers()));
}

TEST(PoseModifierTest, FailurePreservesPoseClocksNotifiesAndRootMotion)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto animation = Animation::Create(
        mesh->GetSkeleton(),
        mesh->GetData().ReferencePose
    );
    ASSERT_TRUE(animation);
    const auto sequence = Sequence::Create(
        "Moving",
        mesh->GetSkeleton(),
        1,
        {{.BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {1, 0, 0}}}}},
        {},
        {{"First", 0.1f}, {"Second", 0.25f}}
    );
    ASSERT_TRUE(animation->SetRootMotionEnabled(true));
    ASSERT_TRUE(animation->Play(SequenceNode::Create(sequence), false));
    ASSERT_TRUE(animation->Update(0.1f));
    ASSERT_EQ(animation->GetNotifyEvents().size(), 1u);
    const auto pose = animation->GetModelPose().Matrices;
    const auto root = animation->GetRootMotionDelta().Translation;
    const std::array<Ref<const PoseModifier>, 2> modifiers{
        CreateRef<TranslationModifier>(1, glm::vec3(2, 0, 0)),
        CreateRef<TranslationModifier>(1, glm::vec3(5, 0, 0), 1, false)
    };
    ASSERT_TRUE(animation->SetPoseModifiers(modifiers));
    EXPECT_FALSE(animation->Update(0.2f));
    EXPECT_EQ(animation->GetModelPose().Matrices, pose);
    EXPECT_FLOAT_EQ(animation->GetTime(), 0.1f);
    EXPECT_EQ(animation->GetRootMotionDelta().Translation, root);
    ASSERT_EQ(animation->GetNotifyEvents().size(), 1u);
    EXPECT_EQ(animation->GetNotifyEvents()[0].GetName(), "First");
    ASSERT_TRUE(animation->SetPoseModifiers({}));
    ASSERT_TRUE(animation->Update(0.2f));
    EXPECT_NEAR(animation->GetTime(), 0.3f, 1e-6f);
    ASSERT_EQ(animation->GetNotifyEvents().size(), 1u);
    EXPECT_EQ(animation->GetNotifyEvents()[0].GetName(), "Second");
}

TEST(PoseModifierTest, RejectsInvalidResultsAndChangesToTheExtractedRootFrame)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto animation = Animation::Create(
        mesh->GetSkeleton(),
        mesh->GetData().ReferencePose
    );
    ASSERT_TRUE(animation);
    const auto before = animation->GetModelPose().Matrices;
    const std::array<Ref<const PoseModifier>, 1> invalid{
        CreateRef<InvalidPoseModifier>()
    };
    ASSERT_TRUE(animation->SetPoseModifiers(invalid));
    EXPECT_FALSE(animation->Update(0));
    EXPECT_EQ(animation->GetModelPose().Matrices, before);
    ASSERT_TRUE(animation->SetPoseModifiers({}));
    ASSERT_TRUE(animation->SetRootMotionEnabled(true));
    const std::array<Ref<const PoseModifier>, 1> moveRoot{
        CreateRef<TranslationModifier>(0, glm::vec3(1, 0, 0))
    };
    ASSERT_TRUE(animation->SetPoseModifiers(moveRoot));
    EXPECT_FALSE(animation->Update(0));
    EXPECT_EQ(animation->GetModelPose().Matrices, before);
}

TEST(PoseModifierTest, StopDetachesCorrectionsAndRestoresTheReferencePose)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto animation = Animation::Create(
        mesh->GetSkeleton(),
        mesh->GetData().ReferencePose
    );
    ASSERT_TRUE(animation);
    const auto before = animation->GetModelPose().Matrices;
    const std::array<Ref<const PoseModifier>, 1> modifiers{
        CreateRef<TranslationModifier>(1, glm::vec3(2, 0, 0))
    };
    ASSERT_TRUE(animation->SetPoseModifiers(modifiers));
    ASSERT_TRUE(animation->Update(0));
    EXPECT_NE(animation->GetModelPose().Matrices, before);
    animation->Stop();
    EXPECT_TRUE(animation->GetPoseModifiers().empty());
    EXPECT_EQ(animation->GetModelPose().Matrices, before);
    ASSERT_TRUE(animation->Update(0));
    EXPECT_EQ(animation->GetModelPose().Matrices, before);
}
