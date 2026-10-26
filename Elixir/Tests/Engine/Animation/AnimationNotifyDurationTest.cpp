#include "AnimationTestNodes.h"
#include "AnimationTestCharacter.h"
#include "AnimationTestPlayback.h"
#include <gtest/gtest.h>
#include <Engine/Animation/BlendSpace1D.h>
#include "AnimationGraphTestDriver.h"
#include "../Mesh/SkinningTestData.h"
#include <limits>

using namespace Elixir;

namespace
{
    using EType = EAnimationNotifyEventType;

    struct SHitPayload { int Damage = 12; };

    class AnimationNotifyDurationTest : public testing::Test
    {
    protected:
        /** @brief Create a two-bone mesh and a one-second timeline. */
        void SetUp() override
        {
            Mesh = SkinningTests::MakeMesh();
            ASSERT_TRUE(Mesh);
            Sequence = Sequence::Create("Windows", Mesh->GetSkeleton(), 1, {});
            ASSERT_TRUE(Sequence);
        }

        /**
         * @brief Copy the fixture clip with one typed window.
         * @param start Window start in seconds.
         * @param end Window end in seconds.
         */
        Ref<Sequence> Window(float start = 0.2f, float end = 0.6f)
        {
            return Sequence->WithNotifies({{"Hitbox", start, end - start, SHitPayload{}}});
        }

        /**
         * @brief Wrap a timed sequence in a montage without fades.
         * @param clip Sequence defining the montage timeline.
         * @param sections Optional section starts and default routes.
         */
        Ref<AnimationMontage> Montage(Ref<const Sequence> clip,
            std::vector<SAnimationMontageSection> sections = {})
        {
            return AnimationMontage::Create({.Name = "Action", .Sequence = std::move(clip),
                .BlendIn = 0, .BlendOut = 0, .Sections = std::move(sections)});
        }

        Ref<SkeletalMesh> Mesh;
        Ref<Sequence> Sequence;
    };
}

TEST_F(AnimationNotifyDurationTest, ValidatesRangesAndPreservesMetadataInEveryClone)
{
    EXPECT_FALSE(Sequence->WithNotifies({{"", 0, 1}}));
    EXPECT_TRUE(Window(0.4f, 0.4f));
    EXPECT_FALSE(Window(0.5f, 0.2f));
    EXPECT_FALSE(Window(-1, 1));
    EXPECT_FALSE(Window(0, 2));
    EXPECT_FALSE(Window(0, std::numeric_limits<float>::quiet_NaN()));
    const auto clip = Window();
    ASSERT_TRUE(clip);
    const auto marked = clip->WithSyncMarkers({{"Contact", 0.4f}});
    ASSERT_TRUE(marked);
    auto notifies = marked->GetNotifies();
    notifies.push_back({"Hit", 0.4f});
    const auto notified = marked->WithNotifies(std::move(notifies));
    ASSERT_TRUE(notified);
    ASSERT_EQ(notified->GetNotifies().size(), 2);
    ASSERT_TRUE(notified->GetNotifies()[0].GetPayload<SHitPayload>());
    EXPECT_EQ(notified->GetNotifies()[0].GetPayload<SHitPayload>()->Damage, 12);
    EXPECT_FALSE(notified->GetNotifies()[0].GetPayload<std::string>());
    const auto stripped = notified->WithNotifies({});
    ASSERT_TRUE(stripped);
    EXPECT_TRUE(stripped->GetNotifies().empty());
    EXPECT_EQ(stripped->GetSyncMarkers().size(), 1);
}

TEST_F(AnimationNotifyDurationTest, WholeWindowInsideOneFrameEmitsBeginUpdateAndEndWithTypedPayload)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Window()), false));
    ASSERT_TRUE(instance.Update(1));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].Type, EType::Begin);
    EXPECT_EQ(events[1].Type, EType::Update);
    EXPECT_EQ(events[2].Type, EType::End);
    EXPECT_NEAR(events[0].TimeOffset, 0.2, 1e-6);
    EXPECT_NEAR(events[1].TimeOffset, 0.6, 1e-6);
    EXPECT_NEAR(events[1].DeltaSeconds, 0.4, 1e-6);
    EXPECT_EQ(events[0].ActivationId, events[2].ActivationId);
    EXPECT_EQ(events[1].LayerName, "Base");
    EXPECT_FALSE(events[2].Cancelled);
    EXPECT_EQ(events[1].GetPayload<SHitPayload>()->Damage, 12);
    ASSERT_TRUE(instance.Update(1));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
}

TEST_F(AnimationNotifyDurationTest, ExactStartBeginsWithoutUpdatingUntilTimeAdvances)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Window(0.25f, 0.5f))));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents()[0].Type, EType::Begin);
    ASSERT_TRUE(instance.Update(0.25f));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events[0].Type, EType::Update);
    EXPECT_DOUBLE_EQ(events[0].DeltaSeconds, 0.25);
    EXPECT_EQ(events[1].Type, EType::End);
}

TEST_F(AnimationNotifyDurationTest, RepeatedCyclesHaveIndependentActivationIdsAndBoundaryOrder)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Window(0, 1))));
    ASSERT_TRUE(instance.Update(2.5f));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 8);
    EXPECT_EQ(events[1].Type, EType::Update);
    EXPECT_EQ(events[2].Type, EType::End);
    EXPECT_EQ(events[3].Type, EType::Begin);
    EXPECT_DOUBLE_EQ(events[2].TimeOffset, 1);
    EXPECT_DOUBLE_EQ(events[3].TimeOffset, 1);
    EXPECT_NE(events[0].ActivationId, events[3].ActivationId);
    EXPECT_NE(events[3].ActivationId, events[6].ActivationId);
    EXPECT_DOUBLE_EQ(events.back().DeltaSeconds, 0.5);
}

TEST_F(AnimationNotifyDurationTest, PauseRetainsWindowsAndResumeUsesTheSameActivation)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto id = instance.GetNotifyEvents()[0].ActivationId;
    instance.Pause();
    ASSERT_TRUE(instance.Update(1));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    instance.Resume();
    instance.GetBaseLayer().GetPlayer().Pause();
    ASSERT_TRUE(instance.Update(1));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    instance.GetBaseLayer().GetPlayer().Resume();
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents()[0].Type, EType::Update);
    EXPECT_EQ(instance.GetNotifyEvents()[0].ActivationId, id);
}

TEST_F(AnimationNotifyDurationTest, SeekCancelsThenReentersAWindowOnTheNextPositiveUpdate)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Window())));
    ASSERT_TRUE(instance.Update(0.3f));
    const auto id = instance.GetNotifyEvents()[0].ActivationId;
    ASSERT_TRUE(instance.Seek(0.4f));
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents()[0].Type, EType::End);
    EXPECT_EQ(instance.GetNotifyEvents()[0].ActivationId, id);
    EXPECT_TRUE(instance.GetNotifyEvents()[0].Cancelled);
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 2);
    EXPECT_EQ(instance.GetNotifyEvents()[0].Type, EType::Begin);
    EXPECT_NE(instance.GetNotifyEvents()[0].ActivationId, id);
}

TEST_F(AnimationNotifyDurationTest, PlaybackReplacementCancelsTheOutgoingWindowDuringFades)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(instance.Play(MakeNode(Sequence), 0.5f));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents()[0].Type, EType::End);
    EXPECT_TRUE(instance.GetNotifyEvents()[0].Cancelled);
}

TEST_F(AnimationNotifyDurationTest, MutingEndsAnActiveWindowAndUnmutingReentersAtCurrentTime)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.AddLayer("Action", mask));
    auto* layer = instance.GetLayer("Action");
    ASSERT_TRUE(layer->GetPlayer().Play(MakeNode(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(layer->SetWeight(0));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_TRUE(instance.GetNotifyEvents()[0].Cancelled);
    ASSERT_TRUE(layer->SetWeight(1));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 2);
    EXPECT_EQ(instance.GetNotifyEvents()[0].Type, EType::Begin);
    EXPECT_NEAR(instance.GetNotifyEvents()[1].DeltaSeconds, 0.1, 1e-6);
}

TEST_F(AnimationNotifyDurationTest, MontageJumpEndsTheWindowWithoutEmittingSkippedStates)
{
    const auto clip = Sequence->WithNotifies({{"Hit", 0, 0.5f}, {"Skipped", 0.5f, 0.25f}});
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.GetPlayer().PlayMontage(Montage(clip,
        {{"Attack", 0}, {"Skip", 0.5f}, {"Recovery", 0.75f}})));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(layer.GetPlayer().JumpToMontageSection("Recovery"));
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents()[0].GetName(), "Hit");
    EXPECT_TRUE(instance.GetNotifyEvents()[0].Cancelled);
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
}

TEST_F(AnimationNotifyDurationTest, AdjacentSectionsRetainTheActivationAcrossTheBoundary)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.GetPlayer().PlayMontage(Montage(Window(0, 1), {{"A", 0}, {"B", 0.5f}})));
    ASSERT_TRUE(instance.Update(0.75f));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].Type, EType::Begin);
    EXPECT_EQ(events[1].Type, EType::Update);
    EXPECT_EQ(events[2].Type, EType::Update);
    EXPECT_EQ(events[0].ActivationId, events[2].ActivationId);
    EXPECT_DOUBLE_EQ(events[1].DeltaSeconds, 0.5);
    EXPECT_DOUBLE_EQ(events[2].DeltaSeconds, 0.25);
}

TEST_F(AnimationNotifyDurationTest, AutomaticNonadjacentLinksEndThenBeginNewActivations)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.GetPlayer().PlayMontage(Montage(Window(0, 1),
        {{"A", 0, "C"}, {"Skip", 0.25f}, {"C", 0.75f, ""}})));
    ASSERT_TRUE(instance.Update(0.375f));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 5);
    EXPECT_EQ(events[2].Type, EType::End);
    EXPECT_TRUE(events[2].Cancelled);
    EXPECT_EQ(events[3].Type, EType::Begin);
    EXPECT_NE(events[0].ActivationId, events[3].ActivationId);
    EXPECT_DOUBLE_EQ(events[3].TimeOffset, 0.25);
}

TEST_F(AnimationNotifyDurationTest, MontageCompletionClosesWindowsThatExtendBeyondTerminalSection)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.GetBaseLayer().GetPlayer().PlayMontage(Montage(Window(0, 1),
        {{"Attack", 0, ""}, {"Unused", 0.5f}})));
    ASSERT_TRUE(instance.Update(1));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events.back().Type, EType::End);
    EXPECT_FALSE(events.back().Cancelled);
    EXPECT_DOUBLE_EQ(events.back().TimeOffset, 0.5);
    EXPECT_DOUBLE_EQ(events[1].DeltaSeconds, 0.5);
}

TEST_F(AnimationNotifyDurationTest, ExplicitMontageExitCancelsImmediatelyAndDoesNotReopenDuringFade)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.GetPlayer().PlayMontage(Montage(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(layer.GetPlayer().StopMontage(0.5f));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_TRUE(instance.GetNotifyEvents()[0].Cancelled);
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
}

TEST_F(AnimationNotifyDurationTest, LayerRemovalRetainsTheEndAndTypedPayloadAfterAssetRelease)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.AddLayer("Action", mask));
    ASSERT_TRUE(instance.GetLayer("Action")->GetPlayer().Play(MakeNode(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(instance.RemoveLayer("Action"));
    instance.Pause();
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    const auto event = instance.GetNotifyEvents()[0];
    EXPECT_EQ(event.LayerName, "Action");
    EXPECT_TRUE(event.Cancelled);
    EXPECT_EQ(event.GetPayload<SHitPayload>()->Damage, 12);
    ASSERT_TRUE(instance.Update(0));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    EXPECT_EQ(event.GetPayload<SHitPayload>()->Damage, 12);
}

TEST_F(AnimationNotifyDurationTest, StopAndManualPoseQueueExactlyOneEndEach)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    instance.Stop();
    instance.Stop();
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_TRUE(instance.GetNotifyEvents()[0].Cancelled);
    ASSERT_TRUE(instance.Play(MakeNode(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(instance.SetLocalPose(Mesh->GetData().ReferencePose));
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_TRUE(instance.GetNotifyEvents()[0].Cancelled);
}

TEST_F(AnimationNotifyDurationTest, BlendSpaceDominantSourceChangeEndsThePreviousWindow)
{
    const auto first = Window(0, 1), second = Window(0, 1);
    const auto space = BlendSpace1D::Create("Space", {{0, first}, {1, second}});
    ASSERT_TRUE(space);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(PlayWithInputs(instance, MakeNode(space), {0}));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(instance.SetInputs({1}));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].Type, EType::End);
    EXPECT_EQ(events[0].Sequence, first);
    EXPECT_TRUE(events[0].Cancelled);
    EXPECT_EQ(events[1].Type, EType::Begin);
    EXPECT_EQ(events[1].Sequence, second);
}

TEST_F(AnimationNotifyDurationTest, MarkerMappingPreservesTheWindowAcrossTheReferenceCycleBoundary)
{
    const auto reference = Sequence->WithSyncMarkers({{"A", 0}, {"B", 0.5f}});
    const auto shifted = Window(0, 0.5f)->WithSyncMarkers({{"A", 0.25f}, {"B", 0.75f}});
    const auto space = BlendSpace1D::Create("Mapped", {{0, reference}, {1, shifted}});
    ASSERT_TRUE(space);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(PlayWithInputs(instance, MakeNode(space), {1}));
    ASSERT_TRUE(instance.Seek(0.875f));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events[0].Type, EType::Begin);
    EXPECT_EQ(events[1].Type, EType::Update);
    EXPECT_DOUBLE_EQ(events[1].DeltaSeconds, 0.25);
    const auto id = events[0].ActivationId;
    ASSERT_TRUE(instance.Update(0.125f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 2);
    EXPECT_EQ(instance.GetNotifyEvents()[0].ActivationId, id);
    EXPECT_EQ(instance.GetNotifyEvents()[1].Type, EType::End);
    EXPECT_FALSE(instance.GetNotifyEvents()[1].Cancelled);
}

TEST_F(AnimationNotifyDurationTest, FailedCompositionPreservesActiveIdentityAndPendingEndsForRetry)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto id = instance.GetNotifyEvents()[0].ActivationId;
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    auto pose = Mesh->GetData().ReferencePose;
    pose.LocalTransforms[0].Translation.x = std::numeric_limits<float>::max();
    for (const auto* name : {"OverflowA", "OverflowB"})
    {
        ASSERT_TRUE(instance.AddLayer(name, mask));
        auto* layer = instance.GetLayer(name);
        ASSERT_TRUE(layer->SetBlendMode(EAnimationLayerBlendMode::Additive));
        ASSERT_TRUE(layer->GetPlayer().SetLocalPose(pose));
    }
    EXPECT_FALSE(instance.Update(0.25f));
    EXPECT_EQ(instance.GetNotifyEvents()[0].ActivationId, id);
    ASSERT_TRUE(instance.RemoveLayer("OverflowA"));
    ASSERT_TRUE(instance.RemoveLayer("OverflowB"));
    ASSERT_TRUE(instance.Seek(0.5f));
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents()[0].ActivationId, id);
    EXPECT_TRUE(instance.GetNotifyEvents()[0].Cancelled);
}

TEST_F(AnimationNotifyDurationTest, AuthoredEventsShareTheExistingCapacityAndFailuresRollBack)
{
    const auto clip = Sequence->WithNotifies(
        std::vector<SAnimationNotify>(1500, {"Window", 0.1f, 0.4f}));
    ASSERT_TRUE(clip);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(clip)));
    EXPECT_FALSE(instance.Update(0.75f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_EQ(instance.GetNotifyEvents().size(), 3000);
    instance.Stop();
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetNotifyEvents().size(), 1500);
}

TEST_F(AnimationNotifyDurationTest, NonloopingFullClipWindowDoesNotReopenAtTheClipEnd)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Window(0, 1)), false));
    ASSERT_TRUE(instance.Update(1));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 3);
    EXPECT_EQ(instance.GetNotifyEvents().back().Type, EType::End);
    EXPECT_FALSE(instance.GetNotifyEvents().back().Cancelled);
    ASSERT_TRUE(instance.Update(1));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
}

TEST_F(AnimationNotifyDurationTest, OverlappingWindowsHaveSeparateIdsAndChronologicalEvents)
{
    const auto clip = Sequence->WithNotifies({{"A", 0.1f, 0.4f}, {"B", 0.25f, 0.5f}});
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(clip)));
    ASSERT_TRUE(instance.Update(1));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 6);
    EXPECT_EQ(events[0].Type, EType::Begin);
    EXPECT_EQ(events[1].Type, EType::Begin);
    EXPECT_NE(events[0].ActivationId, events[1].ActivationId);
    EXPECT_EQ(events[2].Type, EType::Update);
    EXPECT_EQ(events[3].Type, EType::End);
    EXPECT_EQ(events[3].ActivationId, events[0].ActivationId);
    for (size_t i = 1; i < events.size(); ++i)
        EXPECT_LE(events[i - 1].TimeOffset, events[i].TimeOffset);
}

TEST_F(AnimationNotifyDurationTest, InstantaneousEventsAndWindowEventsUseOneAuthoredBudget)
{
    std::vector<SAnimationNotify> notifies(1500, {"Instant", 0.9f});
    notifies.insert(notifies.end(), 1000, {"Window", 0.2f, 0.4f});
    const auto clip = Sequence->WithNotifies(std::move(notifies));
    ASSERT_TRUE(clip);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(clip)));
    EXPECT_FALSE(instance.Update(1));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0.3f));
    EXPECT_EQ(instance.GetNotifyEvents().size(), 2000);
}

TEST_F(AnimationNotifyDurationTest, ClearLayersTransfersEveryPendingEndAndDoesNotEndBase)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.Play(MakeNode(Window(0, 1))));
    for (const auto* name : {"A", "B"})
    {
        ASSERT_TRUE(instance.AddLayer(name, mask));
        ASSERT_TRUE(instance.GetLayer(name)->GetPlayer().Play(MakeNode(Window(0, 1))));
    }
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(instance.GetLayer("A")->SetWeight(0));
    instance.ClearLayers();
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 2);
    for (const auto& event : instance.GetNotifyEvents())
    {
        EXPECT_NE(event.LayerName, "Base");
        EXPECT_EQ(event.Type, EType::End);
        EXPECT_TRUE(event.Cancelled);
    }
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents()[0].LayerName, "Base");
    EXPECT_EQ(instance.GetNotifyEvents()[0].Type, EType::Update);
}

TEST_F(AnimationNotifyDurationTest, UnderlyingPlaybackChangesPreserveMontageWindows)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Sequence)));
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.GetPlayer().PlayMontage(Montage(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto id = instance.GetNotifyEvents()[0].ActivationId;
    ASSERT_TRUE(layer.GetPlayer().Play(MakeNode(Sequence), 0.5f));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents()[0].Type, EType::Update);
    EXPECT_EQ(instance.GetNotifyEvents()[0].ActivationId, id);
    ASSERT_TRUE(layer.GetPlayer().StopMontage(0));
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_TRUE(instance.GetNotifyEvents()[0].Cancelled);
}

TEST_F(AnimationNotifyDurationTest, ReplacingAMontageClosesThenOpensAnotherActivation)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    auto& layer = instance.GetBaseLayer();
    const auto action = Montage(Window(0, 1));
    ASSERT_TRUE(layer.GetPlayer().PlayMontage(action));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto id = instance.GetNotifyEvents()[0].ActivationId;
    ASSERT_TRUE(layer.GetPlayer().PlayMontage(action));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].Type, EType::End);
    EXPECT_TRUE(events[0].Cancelled);
    EXPECT_EQ(events[0].ActivationId, id);
    EXPECT_EQ(events[1].Type, EType::Begin);
    EXPECT_NE(events[1].ActivationId, id);
}

TEST_F(AnimationNotifyDurationTest, NonuniformMarkersMeasureActiveFrameSecondsRatherThanClipSeconds)
{
    const auto reference = Sequence::Create("Reference", Mesh->GetSkeleton(), 2,
        {}, {{"A", 0}, {"B", 1.5f}});
    const auto source = Window(0, 0.5f)->WithSyncMarkers({{"A", 0}, {"B", 0.5f}});
    const auto space = BlendSpace1D::Create("Space", {{0, reference}, {1, source}});
    ASSERT_TRUE(space);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(PlayWithInputs(instance, MakeNode(space), {1}));
    ASSERT_TRUE(instance.Update(0.5f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 2);
    EXPECT_DOUBLE_EQ(instance.GetNotifyEvents()[1].DeltaSeconds, 0.5);
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 2);
    EXPECT_DOUBLE_EQ(instance.GetNotifyEvents()[0].DeltaSeconds, 0.25);
    EXPECT_EQ(instance.GetNotifyEvents()[1].Type, EType::End);
}

TEST_F(AnimationNotifyDurationTest, InvalidCollectionAndFailedPlaybackPreserveTheirOutputs)
{
    const auto clip = Window();
    std::vector<SAnimationNotifyInterval> intervals = {{99}};
    EXPECT_FALSE(clip->CollectNotifyIntervals(-1, 1, true, intervals));
    EXPECT_FALSE(clip->CollectNotifyIntervals(0.5, 0.25, true, intervals));
    EXPECT_FALSE(clip->CollectNotifyIntervals(0, 5000, true, intervals));
    ASSERT_EQ(intervals.size(), 1);
    EXPECT_EQ(intervals[0].NotifyIndex, 99);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FALSE(instance.Play(MakeNode(nullptr)));
    EXPECT_FALSE(instance.Seek(std::numeric_limits<float>::quiet_NaN()));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents()[0].Type, EType::Update);
}

TEST_F(AnimationNotifyDurationTest, SourceWindowsResumeAtTheirCurrentPhaseAfterAMontageReleases)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.GetPlayer().PlayMontage(Montage(Sequence,
        {{"Action", 0, ""}, {"Unused", 0.25f}})));
    ASSERT_TRUE(instance.Update(0.5f));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].Type, EType::End);
    EXPECT_TRUE(events[0].Cancelled);
    EXPECT_EQ(events[1].Type, EType::Begin);
    EXPECT_DOUBLE_EQ(events[1].TimeOffset, 0.25);
    EXPECT_EQ(events[2].Type, EType::Update);
    EXPECT_DOUBLE_EQ(events[2].DeltaSeconds, 0.25);
    EXPECT_DOUBLE_EQ(events[2].TimeOffset, 0.5);
}

TEST_F(AnimationNotifyDurationTest, FailedUpdateKeepsQueuedCancellationForAZeroDeltaRetry)
{
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto id = instance.GetNotifyEvents()[0].ActivationId;
    const auto dense = Sequence->WithNotifies(
        std::vector<SAnimationNotify>(1500, {"Dense", 0, 0.5f}));
    ASSERT_TRUE(instance.Play(MakeNode(dense), false));
    EXPECT_FALSE(instance.Update(1));
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyEvents()[0].ActivationId, id);
    EXPECT_TRUE(instance.GetNotifyEvents()[0].Cancelled);
    ASSERT_TRUE(instance.Update(0));
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_EQ(instance.GetNotifyEvents().size(), 3000);
}

TEST_F(AnimationNotifyDurationTest, InstantAndWindowEventsShareOneChronologicalBatch)
{
    auto clip = Sequence->WithNotifies({
        {"Hitbox", 0.25f, 0.5f, SHitPayload{}},
        {"Start", 0, 0, 7}, {"Impact", 0.5f, 0, std::string("Metal")}
    });
    ASSERT_TRUE(clip);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(clip), false));
    ASSERT_TRUE(instance.Update(1));

    const auto events = instance.GetNotifyEvents();
    const auto layerEvents = instance.GetBaseLayer().GetPlayer().GetNotifyEvents();
    ASSERT_EQ(events.size(), 5);
    ASSERT_EQ(layerEvents.size(), events.size());
    EXPECT_EQ(events[0].Type, EType::Instant);
    EXPECT_EQ(events[0].GetName(), "Start");
    EXPECT_EQ(events[0].ActivationId, 0);
    EXPECT_DOUBLE_EQ(events[0].DeltaSeconds, 0);
    EXPECT_FALSE(events[0].Cancelled);
    ASSERT_TRUE(events[0].GetPayload<int>());
    EXPECT_EQ(*events[0].GetPayload<int>(), 7);
    EXPECT_FALSE(events[0].GetPayload<SHitPayload>());
    EXPECT_EQ(events[1].Type, EType::Begin);
    EXPECT_EQ(events[2].Type, EType::Instant);
    EXPECT_EQ(events[2].GetName(), "Impact");
    EXPECT_EQ(events[3].Type, EType::Update);
    EXPECT_EQ(events[4].Type, EType::End);
    EXPECT_EQ(events[1].ActivationId, events[3].ActivationId);
    EXPECT_EQ(events[1].ActivationId, events[4].ActivationId);
    EXPECT_NE(events[1].ActivationId, 0);

    for (size_t i = 0; i < events.size(); ++i)
    {
        EXPECT_EQ(events[i].Type, layerEvents[i].Type);
        EXPECT_EQ(events[i].GetName(), layerEvents[i].GetName());
        EXPECT_DOUBLE_EQ(events[i].TimeOffset, layerEvents[i].TimeOffset);
        EXPECT_EQ(events[i].LayerName, "Base");
        if (i > 0)
            EXPECT_LE(events[i - 1].TimeOffset, events[i].TimeOffset);
    }

    const auto instant = events[2];
    const auto window = events[4];
    instance.Stop();
    clip.reset();
    ASSERT_TRUE(instant.GetPayload<std::string>());
    EXPECT_EQ(*instant.GetPayload<std::string>(), "Metal");
    ASSERT_TRUE(window.GetPayload<SHitPayload>());
    EXPECT_EQ(window.GetPayload<SHitPayload>()->Damage, 12);
    EXPECT_FALSE(window.GetPayload<int>());
}

TEST_F(AnimationNotifyDurationTest, RejectsInvalidDurationsAndAllowsInstantAtClipEnd)
{
    for (const auto duration : {
        -0.25f, std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()
    })
        EXPECT_FALSE(Sequence->WithNotifies({{"Invalid", 0.25f, duration}}));

    EXPECT_FALSE(Sequence->WithNotifies({{"PastEnd", 0.75f, 0.5f}}));
    EXPECT_FALSE(Sequence->WithNotifies({{"PastEnd", 1, 0.25f}}));
    EXPECT_FALSE(Sequence->WithNotifies({
        {"TooSmall", 0.5f, std::numeric_limits<float>::denorm_min()}
    }));
    EXPECT_TRUE(Sequence->WithNotifies({{"Fits", 0.25f, 0.75f}}));

    const auto clip = Sequence->WithNotifies({{"End", 1}});
    ASSERT_TRUE(clip);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(clip), false));
    ASSERT_TRUE(instance.Update(1));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].Type, EType::Instant);
    EXPECT_DOUBLE_EQ(events[0].TimeOffset, 1);
}

TEST_F(AnimationNotifyDurationTest, MixedBoundaryEventsCloseBeforeTheNextActivation)
{
    const auto clip = Sequence->WithNotifies({
        {"First", 0, 0.5f}, {"Second", 0.5f, 0.5f}, {"Point", 0.5f}
    });
    ASSERT_TRUE(clip);
    const auto instanceHandle = AnimationTestCharacter::Create(Mesh);
    ASSERT_TRUE(instanceHandle);
    auto& instance = *instanceHandle;
    ASSERT_TRUE(instance.Play(MakeNode(clip), false));
    ASSERT_TRUE(instance.Update(0.5f));
    const auto events = instance.GetNotifyEvents();
    ASSERT_EQ(events.size(), 5);
    EXPECT_EQ(events[0].Type, EType::Begin);
    EXPECT_EQ(events[1].Type, EType::Update);
    EXPECT_EQ(events[2].Type, EType::End);
    EXPECT_EQ(events[2].GetName(), "First");
    EXPECT_EQ(events[3].Type, EType::Begin);
    EXPECT_EQ(events[3].GetName(), "Second");
    EXPECT_EQ(events[4].Type, EType::Instant);
    EXPECT_EQ(events[4].GetName(), "Point");
    EXPECT_NE(events[0].ActivationId, events[3].ActivationId);
    const auto activeId = events[3].ActivationId;

    ASSERT_TRUE(instance.Update(0.5f));
    const auto tail = instance.GetNotifyEvents();
    ASSERT_EQ(tail.size(), 2);
    EXPECT_EQ(tail[0].Type, EType::Update);
    EXPECT_EQ(tail[1].Type, EType::End);
    EXPECT_EQ(tail[1].ActivationId, activeId);
    EXPECT_FALSE(tail[1].Cancelled);
}
