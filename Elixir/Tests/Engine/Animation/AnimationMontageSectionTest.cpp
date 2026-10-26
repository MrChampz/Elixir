#include "AnimationTestNodes.h"
#include "AnimationTestCharacter.h"
#include <gtest/gtest.h>
#include <Engine/Animation/AnimationMontage.h>
#include "../Mesh/SkinningTestData.h"
#include <limits>

using namespace Elixir;

namespace
{
    class AnimationMontageSectionTest : public testing::Test
    {
    protected:
        /** @brief Build a timeline with distinct pose ranges and boundary events. */
        void SetUp() override
        {
            Mesh = SkinningTests::MakeMesh();
            ASSERT_TRUE(Mesh);
            Sequence = Sequence::Create("Sections", Mesh->GetSkeleton(), 4, {{
                .BoneIndex = 0,
                .Translations = {{0, {0, 0, 0}}, {1, {1, 0, 0}},
                    {2, {10, 0, 0}}, {3, {12, 0, 0}}, {4, {15, 0, 0}}}
            }}, {}, {{"Start", 0}, {"Boundary", 1}, {"Skipped", 1.5f},
                {"Second", 2}, {"Recovery", 3}, {"End", 4}});
            ASSERT_TRUE(Sequence);
        }

        /**
         * @brief Create sections with configurable default links and fades.
         * @param sections Ordered named section starts.
         * @param in Entrance seconds.
         * @param out Exit seconds.
         */
        Ref<AnimationMontage> Montage(
            std::vector<SAnimationMontageSection> sections,
            float in = 0, float out = 0
        )
        {
            return AnimationMontage::Create({.Name = "Sections", .Sequence = Sequence,
                .BlendIn = in, .BlendOut = out, .Sections = std::move(sections)});
        }

        Ref<SkeletalMesh> Mesh;
        Ref<Sequence> Sequence;
    };
}

TEST_F(AnimationMontageSectionTest, ValidatesNamesStartsAndContinuations)
{
    EXPECT_FALSE(Montage({{"", 0}}));
    EXPECT_FALSE(Montage({{"A", 0}, {"A", 1}}));
    EXPECT_FALSE(Montage({{"A", 1}}));
    EXPECT_FALSE(Montage({{"A", 0}, {"B", 0}}));
    EXPECT_FALSE(Montage({{"A", 0}, {"B", 4}}));
    EXPECT_FALSE(Montage({{"A", 0}, {"B", std::numeric_limits<float>::quiet_NaN()}}));
    EXPECT_FALSE(Montage({{"A", 0, "Missing"}}));
    const auto asset = Montage({{"A", 0}, {"B", 2, ""}});
    ASSERT_TRUE(asset);
    EXPECT_EQ(asset->FindSection("A"), 0);
    EXPECT_EQ(asset->GetNextSection(0), 1);
    EXPECT_FALSE(asset->GetNextSection(1));
    EXPECT_FLOAT_EQ(asset->GetSectionEnd(0), 2);
    EXPECT_FALSE(asset->FindSection("Missing"));
    EXPECT_FLOAT_EQ(asset->GetSectionEnd(8), 0);
    EXPECT_FALSE(asset->GetNextSection(8));
}

TEST_F(AnimationMontageSectionTest, AdjacentSectionsDoNotDuplicateSharedBoundaryEvents)
{
    const auto asset = Montage({{"A", 0}, {"B", 1}, {"C", 2}, {"D", 3}});
    ASSERT_TRUE(asset);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(asset));
    ASSERT_TRUE(instance.Update(2.5f));
    EXPECT_EQ(player.GetCurrentMontageSection(), "C");
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 2.5);
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 11);
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 4);
    EXPECT_EQ(events[0].GetName(), "Start");
    EXPECT_EQ(events[1].GetName(), "Boundary");
    EXPECT_EQ(events[2].GetName(), "Skipped");
    EXPECT_EQ(events[3].GetName(), "Second");
    EXPECT_DOUBLE_EQ(events[3].TimeOffset, 2);
}

TEST_F(AnimationMontageSectionTest, ExactBoundaryCarriesTheDestinationWithoutDuplicatingEvents)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Montage({{"A", 0}, {"B", 1}})));
    ASSERT_TRUE(instance.Update(1));
    EXPECT_EQ(player.GetCurrentMontageSection(), "B");
    ASSERT_EQ(instance.GetNotifyEvents().size(), 2);
    ASSERT_TRUE(instance.Update(0));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
}

TEST_F(AnimationMontageSectionTest, DefaultLinksSkipPoseEventsAndRootMovementOfUnvisitedRanges)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.SetRootMotionEnabled(true));
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Montage({{"A", 0, "C"}, {"Skip", 1},
        {"C", 2, ""}, {"Recovery", 3}})));
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_EQ(player.GetCurrentMontageSection(), "C");
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 2.5);
    EXPECT_NEAR(instance.GetRootMotionDelta().Translation.x, 2, 1e-6f);
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[1].GetName(), "Boundary");
    EXPECT_EQ(events[2].GetName(), "Second");
    EXPECT_DOUBLE_EQ(events[2].TimeOffset, 1);
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FALSE(player.GetMontage());
    EXPECT_NEAR(instance.GetRootMotionDelta().Translation.x, 1, 1e-6f);
}

TEST_F(AnimationMontageSectionTest, InstanceOverridesCanLoopThenExitWithoutChangingSharedAsset)
{
    const auto asset = Montage({{"Loop", 0, "Loop"}, {"Recovery", 1, ""}});
    const auto firstHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(firstHandle);
    auto& first = *firstHandle;
    const auto secondHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(secondHandle);
    auto& second = *secondHandle;
    ASSERT_TRUE(first.GetBaseLayer().GetPlayer().PlayMontage(asset));
    ASSERT_TRUE(second.GetBaseLayer().GetPlayer().PlayMontage(asset));
    ASSERT_TRUE(first.SetRootMotionEnabled(true));
    ASSERT_TRUE(first.Update(2.5f));
    EXPECT_EQ(first.GetBaseLayer().GetPlayer().GetCurrentMontageSection(), "Loop");
    EXPECT_DOUBLE_EQ(first.GetBaseLayer().GetPlayer().GetMontageTime(), 0.5);
    EXPECT_NEAR(first.GetRootMotionDelta().Translation.x, 2.5f, 1e-6f);
    EXPECT_EQ(first.GetNotifyEvents().size(), 5);
    ASSERT_TRUE(first.GetBaseLayer().GetPlayer().SetNextMontageSection("Loop", "Recovery"));
    ASSERT_TRUE(first.Update(0.5f));
    EXPECT_EQ(first.GetBaseLayer().GetPlayer().GetCurrentMontageSection(), "Recovery");
    ASSERT_TRUE(second.Update(1.5f));
    EXPECT_EQ(second.GetBaseLayer().GetPlayer().GetCurrentMontageSection(), "Loop");
    EXPECT_EQ(asset->GetNextSection(0), 0);
}

TEST_F(AnimationMontageSectionTest, JumpIsSilentPreservesPauseAndDoesNotExtractThePoseOffset)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.SetRootMotionEnabled(true));
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Montage({{"A", 0}, {"B", 2}})));
    ASSERT_TRUE(instance.Update(0.25f));
    player.Pause();
    ASSERT_TRUE(player.JumpToMontageSection("B"));
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 2);
    EXPECT_TRUE(player.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 2);
    EXPECT_FLOAT_EQ(instance.GetRootMotionDelta().Translation.x, 0);
    player.Resume();
    ASSERT_TRUE(instance.Update(0));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_NEAR(instance.GetRootMotionDelta().Translation.x, 0.5f, 1e-6f);
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents()[0].GetName(), "Second");
    EXPECT_DOUBLE_EQ(instance.GetNotifyEvents()[0].TimeOffset, 0);
}

TEST_F(AnimationMontageSectionTest, StartingAtNamedSectionUsesASeparateEntranceClock)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    const auto asset = Montage({{"A", 0}, {"B", 2}}, 1, 0);
    ASSERT_TRUE(player.PlayMontage(asset, "B"));
    EXPECT_FLOAT_EQ(player.GetMontageWeight(), 0);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_NEAR(player.GetMontageWeight(), 0.5f, 1e-6f);
    EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 5.5f, 1e-6f);
    EXPECT_EQ(player.GetCurrentMontageSection(), "B");
    EXPECT_FALSE(player.PlayMontage(asset, "Missing"));
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 2.5);
}

TEST_F(AnimationMontageSectionTest, TerminalRouteControlsExitEvenWhenSkippingTheClipEnd)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Montage({{"A", 0, ""}, {"Skip", 1}}, 0, 0.5f)));
    ASSERT_TRUE(instance.Update(0.75f));
    EXPECT_NEAR(player.GetMontageWeight(), 0.5f, 1e-6f);
    EXPECT_FALSE(player.SetNextMontageSection("A", "Skip"));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FALSE(player.GetMontage());
    EXPECT_TRUE(player.GetCurrentMontageSection().empty());
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 0);
}

TEST_F(AnimationMontageSectionTest, ExplicitStopEndsALoopAndRejectsJumpsDuringExit)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Montage({{"Loop", 0, "Loop"}, {"Other", 1}}, 0, 0)));
    ASSERT_TRUE(instance.Update(2.5f));
    ASSERT_TRUE(player.StopMontage(0.5f));
    EXPECT_FALSE(player.JumpToMontageSection("Other"));
    EXPECT_FALSE(player.SetNextMontageSection("Loop", "Other"));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_NEAR(player.GetMontageWeight(), 0.5f, 1e-6f);
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FALSE(player.GetMontage());
}

TEST_F(AnimationMontageSectionTest, ExcessiveSectionIntervalsFailWithoutAdvancingAnyClock)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Montage({{"Loop", 0, "Loop"}, {"Other", 1}})));
    EXPECT_FALSE(instance.Update(4097));
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 0);
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    EXPECT_FLOAT_EQ(instance.GetLocalPose().LocalTransforms[0].Translation.x, 0);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 0.5);
}

TEST_F(AnimationMontageSectionTest, InvalidNamesAndMissingPlaybackLeaveTheCurrentRouteUntouched)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    EXPECT_FALSE(player.SetNextMontageSection("A", "B"));
    EXPECT_FALSE(player.JumpToMontageSection("A"));
    ASSERT_TRUE(player.PlayMontage(Montage({{"A", 0}, {"B", 2}})));
    EXPECT_FALSE(player.SetNextMontageSection("Missing", "B"));
    EXPECT_FALSE(player.SetNextMontageSection("A", "Missing"));
    EXPECT_FALSE(player.JumpToMontageSection("Missing"));
    EXPECT_EQ(player.GetCurrentMontageSection(), "A");
    ASSERT_TRUE(instance.Update(2.5f));
    EXPECT_EQ(player.GetCurrentMontageSection(), "B");
}

TEST_F(AnimationMontageSectionTest, CurrentContinuationCanBeClearedToEndTheAction)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Montage({{"Loop", 0, "Loop"}, {"Other", 1}})));
    ASSERT_TRUE(player.SetNextMontageSection("Loop", ""));
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FALSE(player.GetMontage());
}

TEST_F(AnimationMontageSectionTest, LoopExitRequestedTooLateDoesNotDropTheCurrentWeight)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Montage({{"Loop", 0, "Loop"}, {"Other", 1}}, 0, 0.5f)));
    ASSERT_TRUE(instance.Update(0.75f));
    EXPECT_FALSE(player.SetNextMontageSection("Loop", ""));
    EXPECT_FLOAT_EQ(player.GetMontageWeight(), 1);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_EQ(player.GetCurrentMontageSection(), "Loop");
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 0.25);
}

TEST_F(AnimationMontageSectionTest, BackwardJumpAndLoopPreserveTheEntranceClock)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Montage({{"Loop", 0, "Loop"}, {"Other", 1}}, 2, 0.2f)));
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_NEAR(player.GetMontageWeight(), 0.75f, 1e-6f);
    ASSERT_TRUE(player.JumpToMontageSection("Loop"));
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 0);
    EXPECT_NEAR(player.GetMontageWeight(), 0.75f, 1e-6f);
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_FLOAT_EQ(player.GetMontageWeight(), 1);
}

TEST_F(AnimationMontageSectionTest, NoninterruptibleActionsAllowTheirOwnSectionFlow)
{
    const auto asset = AnimationMontage::Create({.Name = "Committed", .Sequence = Sequence,
        .BlendIn = 0, .BlendOut = 0, .Interruptible = false,
        .Sections = {{"A", 0, "A"}, {"B", 2, ""}}});
    ASSERT_TRUE(asset);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(asset));
    EXPECT_FALSE(player.StopMontage());
    ASSERT_TRUE(player.SetNextMontageSection("A", "B"));
    ASSERT_TRUE(player.JumpToMontageSection("B"));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_EQ(player.GetCurrentMontageSection(), "B");
}

TEST_F(AnimationMontageSectionTest, SourceEventsResumeAtTheEffectiveRouteEnd)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto source = Sequence::Create("Source", Mesh->GetSkeleton(), 1, {},
        {}, {{"Source", 0.25f}});
    ASSERT_TRUE(instance.Play(MakeNode(source)));
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Montage({{"A", 0, ""}, {"Skipped", 1}})));
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_FALSE(player.GetMontage());
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].GetName(), "Start");
    EXPECT_EQ(events[1].GetName(), "Boundary");
    EXPECT_EQ(events[2].GetName(), "Source");
    EXPECT_NEAR(events[2].TimeOffset, 1.25, 1e-6);
}

TEST_F(AnimationMontageSectionTest, EventOverflowPreservesSectionCursorAndRootDelta)
{
    const auto sequence = Sequence::Create("Dense", Mesh->GetSkeleton(), 4, {}, {},
        std::vector<SAnimationNotify>(1024, {"Hit", 0.25f}));
    ASSERT_TRUE(sequence);
    const auto asset = AnimationMontage::Create({.Name = "Dense", .Sequence = sequence,
        .Sections = {{"Loop", 0, "Loop"}, {"Other", 1}}});
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.SetRootMotionEnabled(true));
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(asset));
    ASSERT_TRUE(instance.Update(0.5f));
    const auto eventCount = instance.GetNotifyEvents().size();
    EXPECT_FALSE(instance.Update(5));
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 0.5);
    EXPECT_EQ(player.GetCurrentMontageSection(), "Loop");
    EXPECT_EQ(instance.GetNotifyEvents().size(), eventCount);
    EXPECT_TRUE(instance.GetRootMotionDelta().IsValid());
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_DOUBLE_EQ(player.GetMontageTime(), 0);
}

TEST_F(AnimationMontageSectionTest, ExitEnvelopeSpansTheRemainingRouteAcrossASectionJump)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.SetRootMotionEnabled(true));
    auto& player = instance.GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(Montage({{"A", 0, "Recovery"}, {"Skip", 1},
        {"Recovery", 3, ""}}, 0, 2)));
    ASSERT_TRUE(instance.Update(0.5f));
    EXPECT_NEAR(player.GetMontageWeight(), 0.75f, 1e-6f);
    ASSERT_TRUE(instance.Update(1));
    EXPECT_EQ(player.GetCurrentMontageSection(), "Recovery");
    EXPECT_NEAR(player.GetMontageWeight(), 0.25f, 1e-6f);
    EXPECT_NEAR(instance.GetRootMotionDelta().Translation.x, 0.3125f + 0.5625f, 1e-6f);
}

TEST_F(AnimationMontageSectionTest, ComboRecoveryFadesContinuouslyToTheAdvancingSource)
{
    const auto source = Sequence::Create("Wave", Mesh->GetSkeleton(), 4, {{
        .BoneIndex = 0, .Translations = {{0, {-2, 0, 0}}, {4, {6, 0, 0}}}
    }});
    const auto combo = Sequence::Create("Combo", Mesh->GetSkeleton(), 4, {{
        .BoneIndex = 0,
        .Translations = {{0, {8, 0, 0}}, {0.5f, {16, 0, 0}}, {1, {8, 0, 0}},
            {1.5f, {16, 0, 0}}, {2, {8, 0, 0}}, {2.5f, {16, 0, 0}},
            {3, {8, 0, 0}}, {4, {8, 0, 0}}}
    }});
    const auto asset = AnimationMontage::Create({
        .Name = "Combo", .Sequence = combo, .BlendIn = 0, .BlendOut = 1,
        .Sections = {{"Attack1", 0, "Recovery"}, {"Attack2", 1, "Recovery"},
            {"Attack3", 2, "Recovery"}, {"Recovery", 3}}
    });
    ASSERT_TRUE(source);
    ASSERT_TRUE(asset);
    constexpr float boundaryStep = 1.0f / 4096.0f;
    for (const auto section : {"Attack1", "Attack2", "Attack3"})
    {
        SCOPED_TRACE(section);
        const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
        ASSERT_TRUE(instanceHandle);
        auto& instance = *instanceHandle;
        ASSERT_TRUE(instance.Play(MakeNode(source)));
        auto& player = instance.GetBaseLayer().GetPlayer();
        ASSERT_TRUE(player.PlayMontage(asset, section));
        ASSERT_TRUE(instance.Update(1.0f - boundaryStep));
        const auto beforeRecovery = instance.GetLocalPose().LocalTransforms[0].Translation.x;
        ASSERT_TRUE(instance.Update(boundaryStep));
        EXPECT_EQ(player.GetCurrentMontageSection(), "Recovery");
        EXPECT_NEAR(player.GetMontageWeight(), 1, 1e-5f);
        EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, beforeRecovery, 0.005f);
        ASSERT_TRUE(instance.Update(0.5f));
        EXPECT_NEAR(player.GetMontageWeight(), 0.5f, 1e-5f);
        // The continuing source is at 1.5 seconds (x=1), not restarted at zero (x=-2).
        EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 4.5f, 1e-5f);
        ASSERT_TRUE(instance.Update(0.5f - boundaryStep));
        const auto beforeEnd = instance.GetLocalPose().LocalTransforms[0].Translation.x;
        ASSERT_TRUE(instance.Update(boundaryStep + 0.0001f));
        EXPECT_FALSE(player.GetMontage());
        EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, beforeEnd, 0.002f);
        EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 2.0002f, 1e-5f);
        ASSERT_TRUE(instance.Update(0.1f));
        EXPECT_NEAR(instance.GetLocalPose().LocalTransforms[0].Translation.x, 2.2002f, 1e-5f);
    }
}
