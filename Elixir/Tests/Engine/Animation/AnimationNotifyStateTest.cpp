#include <gtest/gtest.h>
#include <Engine/Animation/BlendSpace1D.h>
#include <Engine/Animation/AnimationControllerInstance.h>
#include "../Mesh/SkinningTestData.h"
#include <limits>

using namespace Elixir;

namespace
{
    using EType = EAnimationNotifyStateEventType;

    struct SHitPayload { int Damage = 12; };

    class AnimationNotifyStateTest : public testing::Test
    {
    protected:
        /** @brief Create a two-bone mesh and a one-second timeline. */
        void SetUp() override
        {
            Mesh = SkinningTests::MakeMesh();
            ASSERT_TRUE(Mesh);
            Sequence = AnimationSequence::Create("Windows", Mesh->GetSkeleton(), 1, {});
            ASSERT_TRUE(Sequence);
        }

        /**
         * @brief Copy the fixture clip with one typed window.
         * @param start Window start in seconds.
         * @param end Window end in seconds.
         */
        Ref<AnimationSequence> Window(float start = 0.2f, float end = 0.6f)
        {
            return Sequence->WithNotifyStates({{"Hitbox", start, end, SHitPayload{}}});
        }

        /**
         * @brief Wrap a timed sequence in a montage without fades.
         * @param clip Sequence defining the montage timeline.
         * @param sections Optional section starts and default routes.
         */
        Ref<AnimationMontage> Montage(Ref<const AnimationSequence> clip,
            std::vector<SAnimationMontageSection> sections = {})
        {
            return AnimationMontage::Create({.Name = "Action", .Sequence = std::move(clip),
                .BlendIn = 0, .BlendOut = 0, .Sections = std::move(sections)});
        }

        Ref<SkeletalMesh> Mesh;
        Ref<AnimationSequence> Sequence;
    };
}

TEST_F(AnimationNotifyStateTest, ValidatesRangesAndPreservesMetadataInEveryClone)
{
    EXPECT_FALSE(Sequence->WithNotifyStates({{"", 0, 1}}));
    EXPECT_FALSE(Window(0.4f, 0.4f));
    EXPECT_FALSE(Window(0.5f, 0.2f));
    EXPECT_FALSE(Window(-1, 1));
    EXPECT_FALSE(Window(0, 2));
    EXPECT_FALSE(Window(0, std::numeric_limits<float>::quiet_NaN()));
    const auto clip = Window();
    ASSERT_TRUE(clip);
    const auto marked = clip->WithSyncMarkers({{"Contact", 0.4f}});
    ASSERT_TRUE(marked);
    const auto notified = marked->WithNotifies({{"Hit", 0.4f}});
    ASSERT_TRUE(notified);
    ASSERT_EQ(notified->GetNotifyStates().size(), 1);
    ASSERT_TRUE(notified->GetNotifyStates()[0].GetPayload<SHitPayload>());
    EXPECT_EQ(notified->GetNotifyStates()[0].GetPayload<SHitPayload>()->Damage, 12);
    EXPECT_FALSE(notified->GetNotifyStates()[0].GetPayload<std::string>());
    const auto stripped = notified->WithNotifyStates({});
    ASSERT_TRUE(stripped);
    EXPECT_TRUE(stripped->GetNotifyStates().empty());
    EXPECT_EQ(stripped->GetNotifies().size(), 1);
    EXPECT_EQ(stripped->GetSyncMarkers().size(), 1);
}

TEST_F(AnimationNotifyStateTest, WholeWindowInsideOneFrameEmitsBeginUpdateAndEndWithTypedPayload)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Window(), false));
    ASSERT_TRUE(instance.Update(1));
    const auto events = instance.GetNotifyStateEvents();
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
    EXPECT_EQ(events[1].GetNotifyState().GetPayload<SHitPayload>()->Damage, 12);
    ASSERT_TRUE(instance.Update(1));
    EXPECT_TRUE(instance.GetNotifyStateEvents().empty());
}

TEST_F(AnimationNotifyStateTest, ExactStartBeginsWithoutUpdatingUntilTimeAdvances)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Window(0.25f, 0.5f)));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_TRUE(instance.GetNotifyStateEvents().empty());
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].Type, EType::Begin);
    ASSERT_TRUE(instance.Update(0.25f));
    const auto events = instance.GetNotifyStateEvents();
    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events[0].Type, EType::Update);
    EXPECT_DOUBLE_EQ(events[0].DeltaSeconds, 0.25);
    EXPECT_EQ(events[1].Type, EType::End);
}

TEST_F(AnimationNotifyStateTest, RepeatedCyclesHaveIndependentActivationIdsAndBoundaryOrder)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Window(0, 1)));
    ASSERT_TRUE(instance.Update(2.5f));
    const auto events = instance.GetNotifyStateEvents();
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

TEST_F(AnimationNotifyStateTest, PauseRetainsWindowsAndResumeUsesTheSameActivation)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Window(0, 1)));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto id = instance.GetNotifyStateEvents()[0].ActivationId;
    instance.Pause();
    ASSERT_TRUE(instance.Update(1));
    EXPECT_TRUE(instance.GetNotifyStateEvents().empty());
    instance.Resume();
    instance.GetBaseLayer().Pause();
    ASSERT_TRUE(instance.Update(1));
    EXPECT_TRUE(instance.GetNotifyStateEvents().empty());
    instance.GetBaseLayer().Resume();
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].Type, EType::Update);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].ActivationId, id);
}

TEST_F(AnimationNotifyStateTest, SeekCancelsThenReentersAWindowOnTheNextPositiveUpdate)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Window()));
    ASSERT_TRUE(instance.Update(0.3f));
    const auto id = instance.GetNotifyStateEvents()[0].ActivationId;
    ASSERT_TRUE(instance.Seek(0.4f));
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].Type, EType::End);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].ActivationId, id);
    EXPECT_TRUE(instance.GetNotifyStateEvents()[0].Cancelled);
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 2);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].Type, EType::Begin);
    EXPECT_NE(instance.GetNotifyStateEvents()[0].ActivationId, id);
}

TEST_F(AnimationNotifyStateTest, PlaybackReplacementCancelsTheOutgoingWindowDuringFades)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Window(0, 1)));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(instance.Play(Sequence, 0.5f));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].Type, EType::End);
    EXPECT_TRUE(instance.GetNotifyStateEvents()[0].Cancelled);
}

TEST_F(AnimationNotifyStateTest, MutingEndsAnActiveWindowAndUnmutingReentersAtCurrentTime)
{
    SkeletalMeshInstance instance(Mesh);
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.AddLayer("Action", mask));
    auto* layer = instance.GetLayer("Action");
    ASSERT_TRUE(layer->Play(Window(0, 1)));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(layer->SetWeight(0));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_TRUE(instance.GetNotifyStateEvents()[0].Cancelled);
    ASSERT_TRUE(layer->SetWeight(1));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 2);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].Type, EType::Begin);
    EXPECT_NEAR(instance.GetNotifyStateEvents()[1].DeltaSeconds, 0.1, 1e-6);
}

TEST_F(AnimationNotifyStateTest, MontageJumpEndsTheWindowWithoutEmittingSkippedStates)
{
    const auto clip = Sequence->WithNotifyStates({{"Hit", 0, 0.5f}, {"Skipped", 0.5f, 0.75f}});
    SkeletalMeshInstance instance(Mesh);
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.PlayMontage(Montage(clip,
        {{"Attack", 0}, {"Skip", 0.5f}, {"Recovery", 0.75f}})));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(layer.JumpToMontageSection("Recovery"));
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].GetNotifyState().Name, "Hit");
    EXPECT_TRUE(instance.GetNotifyStateEvents()[0].Cancelled);
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_TRUE(instance.GetNotifyStateEvents().empty());
}

TEST_F(AnimationNotifyStateTest, AdjacentSectionsRetainTheActivationAcrossTheBoundary)
{
    SkeletalMeshInstance instance(Mesh);
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.PlayMontage(Montage(Window(0, 1), {{"A", 0}, {"B", 0.5f}})));
    ASSERT_TRUE(instance.Update(0.75f));
    const auto events = instance.GetNotifyStateEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].Type, EType::Begin);
    EXPECT_EQ(events[1].Type, EType::Update);
    EXPECT_EQ(events[2].Type, EType::Update);
    EXPECT_EQ(events[0].ActivationId, events[2].ActivationId);
    EXPECT_DOUBLE_EQ(events[1].DeltaSeconds, 0.5);
    EXPECT_DOUBLE_EQ(events[2].DeltaSeconds, 0.25);
}

TEST_F(AnimationNotifyStateTest, AutomaticNonadjacentLinksEndThenBeginNewActivations)
{
    SkeletalMeshInstance instance(Mesh);
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.PlayMontage(Montage(Window(0, 1),
        {{"A", 0, "C"}, {"Skip", 0.25f}, {"C", 0.75f, ""}})));
    ASSERT_TRUE(instance.Update(0.375f));
    const auto events = instance.GetNotifyStateEvents();
    ASSERT_EQ(events.size(), 5);
    EXPECT_EQ(events[2].Type, EType::End);
    EXPECT_TRUE(events[2].Cancelled);
    EXPECT_EQ(events[3].Type, EType::Begin);
    EXPECT_NE(events[0].ActivationId, events[3].ActivationId);
    EXPECT_DOUBLE_EQ(events[3].TimeOffset, 0.25);
}

TEST_F(AnimationNotifyStateTest, MontageCompletionClosesWindowsThatExtendBeyondTerminalSection)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.GetBaseLayer().PlayMontage(Montage(Window(0, 1),
        {{"Attack", 0, ""}, {"Unused", 0.5f}})));
    ASSERT_TRUE(instance.Update(1));
    const auto events = instance.GetNotifyStateEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events.back().Type, EType::End);
    EXPECT_FALSE(events.back().Cancelled);
    EXPECT_DOUBLE_EQ(events.back().TimeOffset, 0.5);
    EXPECT_DOUBLE_EQ(events[1].DeltaSeconds, 0.5);
}

TEST_F(AnimationNotifyStateTest, ExplicitMontageExitCancelsImmediatelyAndDoesNotReopenDuringFade)
{
    SkeletalMeshInstance instance(Mesh);
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.PlayMontage(Montage(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(layer.StopMontage(0.5f));
    ASSERT_TRUE(instance.Update(0.1f));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_TRUE(instance.GetNotifyStateEvents()[0].Cancelled);
    ASSERT_TRUE(instance.Update(0.1f));
    EXPECT_TRUE(instance.GetNotifyStateEvents().empty());
}

TEST_F(AnimationNotifyStateTest, LayerRemovalRetainsTheEndAndTypedPayloadAfterAssetRelease)
{
    SkeletalMeshInstance instance(Mesh);
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.AddLayer("Action", mask));
    ASSERT_TRUE(instance.GetLayer("Action")->Play(Window(0, 1)));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(instance.RemoveLayer("Action"));
    instance.Pause();
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    const auto event = instance.GetNotifyStateEvents()[0];
    EXPECT_EQ(event.LayerName, "Action");
    EXPECT_TRUE(event.Cancelled);
    EXPECT_EQ(event.GetNotifyState().GetPayload<SHitPayload>()->Damage, 12);
    ASSERT_TRUE(instance.Update(0));
    EXPECT_TRUE(instance.GetNotifyStateEvents().empty());
    EXPECT_EQ(event.GetNotifyState().GetPayload<SHitPayload>()->Damage, 12);
}

TEST_F(AnimationNotifyStateTest, StopAndManualPoseQueueExactlyOneEndEach)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Window(0, 1)));
    ASSERT_TRUE(instance.Update(0.25f));
    instance.Stop();
    instance.Stop();
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_TRUE(instance.GetNotifyStateEvents()[0].Cancelled);
    ASSERT_TRUE(instance.Play(Window(0, 1)));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(instance.SetLocalPose(Mesh->GetData().ReferencePose));
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_TRUE(instance.GetNotifyStateEvents()[0].Cancelled);
}

TEST_F(AnimationNotifyStateTest, BlendSpaceDominantSourceChangeEndsThePreviousWindow)
{
    const auto first = Window(0, 1), second = Window(0, 1);
    const auto space = BlendSpace1D::Create("Space", {{0, first}, {1, second}});
    ASSERT_TRUE(space);
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(space, {0}));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(instance.SetBlendSpaceValue({1}));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto events = instance.GetNotifyStateEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].Type, EType::End);
    EXPECT_EQ(events[0].Sequence, first);
    EXPECT_TRUE(events[0].Cancelled);
    EXPECT_EQ(events[1].Type, EType::Begin);
    EXPECT_EQ(events[1].Sequence, second);
}

TEST_F(AnimationNotifyStateTest, MarkerMappingPreservesTheWindowAcrossTheReferenceCycleBoundary)
{
    const auto reference = Sequence->WithSyncMarkers({{"A", 0}, {"B", 0.5f}});
    const auto shifted = Window(0, 0.5f)->WithSyncMarkers({{"A", 0.25f}, {"B", 0.75f}});
    const auto space = BlendSpace1D::Create("Mapped", {{0, reference}, {1, shifted}});
    ASSERT_TRUE(space);
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(space, {1}));
    ASSERT_TRUE(instance.Seek(0.875f));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto events = instance.GetNotifyStateEvents();
    ASSERT_EQ(events.size(), 2);
    EXPECT_EQ(events[0].Type, EType::Begin);
    EXPECT_EQ(events[1].Type, EType::Update);
    EXPECT_DOUBLE_EQ(events[1].DeltaSeconds, 0.25);
    const auto id = events[0].ActivationId;
    ASSERT_TRUE(instance.Update(0.125f));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 2);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].ActivationId, id);
    EXPECT_EQ(instance.GetNotifyStateEvents()[1].Type, EType::End);
    EXPECT_FALSE(instance.GetNotifyStateEvents()[1].Cancelled);
}

TEST_F(AnimationNotifyStateTest, FailedCompositionPreservesActiveIdentityAndPendingEndsForRetry)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Window(0, 1)));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto id = instance.GetNotifyStateEvents()[0].ActivationId;
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    auto pose = Mesh->GetData().ReferencePose;
    pose[0].Translation.x = std::numeric_limits<float>::max();
    for (const auto* name : {"OverflowA", "OverflowB"})
    {
        ASSERT_TRUE(instance.AddLayer(name, mask));
        auto* layer = instance.GetLayer(name);
        ASSERT_TRUE(layer->SetBlendMode(EAnimationLayerBlendMode::Additive));
        ASSERT_TRUE(layer->SetLocalPose(pose));
    }
    EXPECT_FALSE(instance.Update(0.25f));
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].ActivationId, id);
    ASSERT_TRUE(instance.RemoveLayer("OverflowA"));
    ASSERT_TRUE(instance.RemoveLayer("OverflowB"));
    ASSERT_TRUE(instance.Seek(0.5f));
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].ActivationId, id);
    EXPECT_TRUE(instance.GetNotifyStateEvents()[0].Cancelled);
}

TEST_F(AnimationNotifyStateTest, AuthoredEventsShareTheExistingCapacityAndFailuresRollBack)
{
    const auto clip = Sequence->WithNotifyStates(
        std::vector<SAnimationNotifyState>(1500, {"Window", 0.1f, 0.5f}));
    ASSERT_TRUE(clip);
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(clip));
    EXPECT_FALSE(instance.Update(0.75f));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_TRUE(instance.GetNotifyStateEvents().empty());
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_EQ(instance.GetNotifyStateEvents().size(), 3000);
    instance.Stop();
    ASSERT_TRUE(instance.Update(0));
    EXPECT_EQ(instance.GetNotifyStateEvents().size(), 1500);
}

TEST_F(AnimationNotifyStateTest, NonloopingFullClipWindowDoesNotReopenAtTheClipEnd)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Window(0, 1), false));
    ASSERT_TRUE(instance.Update(1));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 3);
    EXPECT_EQ(instance.GetNotifyStateEvents().back().Type, EType::End);
    EXPECT_FALSE(instance.GetNotifyStateEvents().back().Cancelled);
    ASSERT_TRUE(instance.Update(1));
    EXPECT_TRUE(instance.GetNotifyStateEvents().empty());
}

TEST_F(AnimationNotifyStateTest, OverlappingWindowsHaveSeparateIdsAndChronologicalEvents)
{
    const auto clip = Sequence->WithNotifyStates({{"A", 0.1f, 0.5f}, {"B", 0.25f, 0.75f}});
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(clip));
    ASSERT_TRUE(instance.Update(1));
    const auto events = instance.GetNotifyStateEvents();
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

TEST_F(AnimationNotifyStateTest, InstantaneousEventsAndWindowEventsUseOneAuthoredBudget)
{
    const auto clip = Sequence->WithNotifies(
        std::vector<SAnimationNotify>(1500, {"Instant", 0.9f}))->WithNotifyStates(
            std::vector<SAnimationNotifyState>(1000, {"Window", 0.2f, 0.6f}));
    ASSERT_TRUE(clip);
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(clip));
    EXPECT_FALSE(instance.Update(1));
    EXPECT_FLOAT_EQ(instance.GetTime(), 0);
    EXPECT_TRUE(instance.GetNotifyStateEvents().empty());
    EXPECT_TRUE(instance.GetNotifyEvents().empty());
    ASSERT_TRUE(instance.Update(0.3f));
    EXPECT_EQ(instance.GetNotifyStateEvents().size(), 2000);
}

TEST_F(AnimationNotifyStateTest, ClearLayersTransfersEveryPendingEndAndDoesNotEndBase)
{
    SkeletalMeshInstance instance(Mesh);
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    ASSERT_TRUE(instance.Play(Window(0, 1)));
    for (const auto* name : {"A", "B"})
    {
        ASSERT_TRUE(instance.AddLayer(name, mask));
        ASSERT_TRUE(instance.GetLayer(name)->Play(Window(0, 1)));
    }
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_TRUE(instance.GetLayer("A")->SetWeight(0));
    instance.ClearLayers();
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 2);
    for (const auto& event : instance.GetNotifyStateEvents())
    {
        EXPECT_NE(event.LayerName, "Base");
        EXPECT_EQ(event.Type, EType::End);
        EXPECT_TRUE(event.Cancelled);
    }
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].LayerName, "Base");
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].Type, EType::Update);
}

TEST_F(AnimationNotifyStateTest, UnderlyingPlaybackChangesPreserveMontageWindows)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Sequence));
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.PlayMontage(Montage(Window(0, 1))));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto id = instance.GetNotifyStateEvents()[0].ActivationId;
    ASSERT_TRUE(layer.Play(Sequence, 0.5f));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].Type, EType::Update);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].ActivationId, id);
    ASSERT_TRUE(layer.StopMontage(0));
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_TRUE(instance.GetNotifyStateEvents()[0].Cancelled);
}

TEST_F(AnimationNotifyStateTest, ReplacingAMontageClosesThenOpensAnotherActivation)
{
    SkeletalMeshInstance instance(Mesh);
    auto& layer = instance.GetBaseLayer();
    const auto action = Montage(Window(0, 1));
    ASSERT_TRUE(layer.PlayMontage(action));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto id = instance.GetNotifyStateEvents()[0].ActivationId;
    ASSERT_TRUE(layer.PlayMontage(action));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto events = instance.GetNotifyStateEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].Type, EType::End);
    EXPECT_TRUE(events[0].Cancelled);
    EXPECT_EQ(events[0].ActivationId, id);
    EXPECT_EQ(events[1].Type, EType::Begin);
    EXPECT_NE(events[1].ActivationId, id);
}

TEST_F(AnimationNotifyStateTest, NonuniformMarkersMeasureActiveFrameSecondsRatherThanClipSeconds)
{
    const auto reference = AnimationSequence::Create("Reference", Mesh->GetSkeleton(), 2,
        {}, {{"A", 0}, {"B", 1.5f}});
    const auto source = Window(0, 0.5f)->WithSyncMarkers({{"A", 0}, {"B", 0.5f}});
    const auto space = BlendSpace1D::Create("Space", {{0, reference}, {1, source}});
    ASSERT_TRUE(space);
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(space, {1}));
    ASSERT_TRUE(instance.Update(0.5f));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 2);
    EXPECT_DOUBLE_EQ(instance.GetNotifyStateEvents()[1].DeltaSeconds, 0.5);
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 2);
    EXPECT_DOUBLE_EQ(instance.GetNotifyStateEvents()[0].DeltaSeconds, 0.25);
    EXPECT_EQ(instance.GetNotifyStateEvents()[1].Type, EType::End);
}

TEST_F(AnimationNotifyStateTest, InvalidCollectionAndFailedPlaybackPreserveTheirOutputs)
{
    const auto clip = Window();
    std::vector<SAnimationNotifyStateInterval> intervals = {{99}};
    EXPECT_FALSE(clip->CollectNotifyStateIntervals(-1, 1, true, intervals));
    EXPECT_FALSE(clip->CollectNotifyStateIntervals(0.5, 0.25, true, intervals));
    EXPECT_FALSE(clip->CollectNotifyStateIntervals(0, 5000, true, intervals));
    ASSERT_EQ(intervals.size(), 1);
    EXPECT_EQ(intervals[0].NotifyStateIndex, 99);
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Window(0, 1)));
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_FALSE(instance.Play(nullptr));
    EXPECT_FALSE(instance.Seek(std::numeric_limits<float>::quiet_NaN()));
    ASSERT_TRUE(instance.Update(0.25f));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].Type, EType::Update);
}

TEST_F(AnimationNotifyStateTest, SourceWindowsResumeAtTheirCurrentPhaseAfterAMontageReleases)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Window(0, 1)));
    ASSERT_TRUE(instance.Update(0.25f));
    auto& layer = instance.GetBaseLayer();
    ASSERT_TRUE(layer.PlayMontage(Montage(Sequence,
        {{"Action", 0, ""}, {"Unused", 0.25f}})));
    ASSERT_TRUE(instance.Update(0.5f));
    const auto events = instance.GetNotifyStateEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].Type, EType::End);
    EXPECT_TRUE(events[0].Cancelled);
    EXPECT_EQ(events[1].Type, EType::Begin);
    EXPECT_DOUBLE_EQ(events[1].TimeOffset, 0.25);
    EXPECT_EQ(events[2].Type, EType::Update);
    EXPECT_DOUBLE_EQ(events[2].DeltaSeconds, 0.25);
    EXPECT_DOUBLE_EQ(events[2].TimeOffset, 0.5);
}

TEST_F(AnimationNotifyStateTest, FailedUpdateKeepsQueuedCancellationForAZeroDeltaRetry)
{
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(Window(0, 1)));
    ASSERT_TRUE(instance.Update(0.25f));
    const auto id = instance.GetNotifyStateEvents()[0].ActivationId;
    const auto dense = Sequence->WithNotifyStates(
        std::vector<SAnimationNotifyState>(1500, {"Dense", 0, 0.5f}));
    ASSERT_TRUE(instance.Play(dense, false));
    EXPECT_FALSE(instance.Update(1));
    ASSERT_TRUE(instance.Update(0));
    ASSERT_EQ(instance.GetNotifyStateEvents().size(), 1);
    EXPECT_EQ(instance.GetNotifyStateEvents()[0].ActivationId, id);
    EXPECT_TRUE(instance.GetNotifyStateEvents()[0].Cancelled);
    ASSERT_TRUE(instance.Update(0));
    EXPECT_TRUE(instance.GetNotifyStateEvents().empty());
    ASSERT_TRUE(instance.Update(0.25f));
    EXPECT_EQ(instance.GetNotifyStateEvents().size(), 3000);
}
