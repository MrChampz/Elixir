#include <gtest/gtest.h>

#include <Engine/Animation/AnimationControllerInstance.h>
#include <Engine/Animation/BlendSpace1D.h>
#include <Engine/Animation/BlendSpace2D.h>
#include "../Mesh/SkinningTestData.h"

#include <limits>

using namespace Elixir;

namespace
{
    class AnimationSyncMarkerTest : public testing::Test
    {
    protected:
        /** @brief Create cycles whose marker times differ from normalized alignment. */
        void SetUp() override
        {
            Mesh = SkinningTests::MakeMesh();
            ASSERT_TRUE(Mesh);
            Reference = MakeSequence("Walk", 4, {{ "RightFootPlant", 3 },
                { "LeftFootPlant", 1 }});
            Target = MakeSequence("Run", 8, {{ "LeftFootPlant", 1 },
                { "RightFootPlant", 5 }});
            Rotated = MakeSequence("Strafe", 6, {{ "RightFootPlant", 0.75f },
                { "LeftFootPlant", 4.5f }});
            ASSERT_TRUE(Reference);
            ASSERT_TRUE(Target);
            ASSERT_TRUE(Rotated);
        }

        /**
         * @brief Create a clip whose root translation exposes its sample time.
         * @param name Clip name.
         * @param duration Duration in seconds.
         * @param markers Optional synchronization markers.
         */
        Ref<AnimationSequence> MakeSequence(std::string name, float duration,
            std::vector<SAnimationSyncMarker> markers = {}) const
        {
            return AnimationSequence::Create(std::move(name), Mesh->GetSkeleton(), duration, {
                { .BoneIndex = 0, .Translations = {
                    { 0, { 0, 0, 0 } }, { duration, { duration, 0, 0 } },
                } },
            }, std::move(markers));
        }

        Ref<SkeletalMesh> Mesh;
        Ref<AnimationSequence> Reference;
        Ref<AnimationSequence> Target;
        Ref<AnimationSequence> Rotated;
    };
}

TEST_F(AnimationSyncMarkerTest, SortsMarkersAndMapsExactContactsAndIntermediateTimes)
{
    ASSERT_EQ(Reference->GetSyncMarkers().size(), 2);
    EXPECT_EQ(Reference->GetSyncMarkers()[0].Name, "LeftFootPlant");
    EXPECT_FLOAT_EQ(Reference->GetSyncMarkers()[0].Time, 1);
    EXPECT_TRUE(Target->CanSynchronizeWith(*Reference));
    EXPECT_TRUE(Reference->CanSynchronizeWith(*Target));
    EXPECT_FLOAT_EQ(Target->GetSynchronizedTime(0.25f, *Reference), 1);
    EXPECT_FLOAT_EQ(Target->GetSynchronizedTime(0.5f, *Reference), 3);
    EXPECT_FLOAT_EQ(Target->GetSynchronizedTime(0.75f, *Reference), 5);
    EXPECT_FLOAT_EQ(Target->GetSynchronizedTime(0.875f, *Reference), 6);
    for (const float phase : { 0.0f, 0.125f, 0.5f, 0.875f, 1.0f })
        EXPECT_FLOAT_EQ(Reference->GetSynchronizedTime(phase, *Reference), phase * 4);
}

TEST_F(AnimationSyncMarkerTest, MapsAcrossTheCycleBoundaryAndSupportsRotatedMarkerOrder)
{
    EXPECT_FLOAT_EQ(Target->GetSynchronizedTime(0, *Reference), 7);
    EXPECT_FLOAT_EQ(Target->GetSynchronizedTime(1, *Reference), 7);
    EXPECT_FLOAT_EQ(Target->GetSynchronizedTime(-2, *Reference), 7);
    EXPECT_FLOAT_EQ(Target->GetSynchronizedTime(2, *Reference), 7);
    EXPECT_FLOAT_EQ(Target->GetSynchronizedTime(0.125f, *Reference), 8);
    EXPECT_NEAR(Target->GetSynchronizedTime(0.1251f, *Reference), 0.0008f, 1e-6f);
    EXPECT_TRUE(Rotated->CanSynchronizeWith(*Reference));
    EXPECT_FLOAT_EQ(Rotated->GetSynchronizedTime(0.25f, *Reference), 4.5f);
    EXPECT_FLOAT_EQ(Rotated->GetSynchronizedTime(0.75f, *Reference), 0.75f);
    EXPECT_FLOAT_EQ(Rotated->GetSynchronizedTime(0.5f, *Reference), 5.625f);
    EXPECT_FLOAT_EQ(Rotated->GetSynchronizedTime(0, *Reference), 2.625f);
    EXPECT_FLOAT_EQ(Rotated->GetSynchronizedTime(1, *Reference), 2.625f);
}

TEST_F(AnimationSyncMarkerTest, SupportsSingleMarkersAndPreservesAlignedCycleEndpoints)
{
    const auto first = MakeSequence("First", 4, {{ "Contact", 1 }});
    const auto second = MakeSequence("Second", 8, {{ "Contact", 3 }});
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_TRUE(first->CanSynchronizeWith(*second));
    EXPECT_FLOAT_EQ(second->GetSynchronizedTime(0.25f, *first), 3);
    EXPECT_FLOAT_EQ(second->GetSynchronizedTime(0, *first), 1);
    EXPECT_FLOAT_EQ(second->GetSynchronizedTime(1, *first), 1);
    const auto aligned = MakeSequence("Aligned", 8, {
        { "LeftFootPlant", 2 }, { "RightFootPlant", 6 },
    });
    ASSERT_TRUE(aligned);
    EXPECT_FLOAT_EQ(aligned->GetSynchronizedTime(0, *Reference), 0);
    EXPECT_FLOAT_EQ(aligned->GetSynchronizedTime(1, *Reference), 8);
    EXPECT_FLOAT_EQ(aligned->GetSynchronizedTime(0.5f, *Reference), 4);
}

TEST_F(AnimationSyncMarkerTest, MissingOrIncompatibleTracksUseNormalizedTime)
{
    const auto plain = MakeSequence("Plain", 8);
    const auto missing = MakeSequence("Missing", 8, {{ "LeftFootPlant", 1 }});
    const auto renamed = MakeSequence("Renamed", 8, {{ "Left", 1 }, { "Right", 5 }});
    const auto ordered = MakeSequence("Ordered", 8, {{ "A", 1 }, { "B", 3 }, { "C", 5 }});
    const auto reversed = MakeSequence("Reversed", 8, {{ "A", 1 }, { "C", 3 }, { "B", 5 }});
    for (const auto& clip : { plain, missing, renamed })
    {
        ASSERT_TRUE(clip);
        EXPECT_FALSE(clip->CanSynchronizeWith(*Reference));
        EXPECT_FALSE(Reference->CanSynchronizeWith(*clip));
        EXPECT_FLOAT_EQ(clip->GetSynchronizedTime(0.25f, *Reference), 2);
    }
    EXPECT_FALSE(ordered->CanSynchronizeWith(*reversed));
    EXPECT_FLOAT_EQ(ordered->GetSynchronizedTime(0.25f, *reversed), 2);
    EXPECT_FLOAT_EQ(Target->GetSynchronizedTime(0.25f, *plain), 2);
    EXPECT_FLOAT_EQ(Target->GetSynchronizedTime(
        std::numeric_limits<float>::quiet_NaN(), *Reference), 0);
}

TEST_F(AnimationSyncMarkerTest, RejectsInvalidMarkersAndMarkersOnStaticSequences)
{
    const auto create = [&](std::vector<SAnimationSyncMarker> markers)
    {
        return AnimationSequence::Create("Invalid", Mesh->GetSkeleton(), 4, {},
            std::move(markers));
    };
    EXPECT_FALSE(create({{ "", 1 }}));
    EXPECT_FALSE(create({{ "Left", -1 }}));
    EXPECT_FALSE(create({{ "Left", 4 }}));
    EXPECT_FALSE(create({{ "Left", 5 }}));
    EXPECT_FALSE(create({{ "Left", 1 }, { "Left", 2 }}));
    EXPECT_FALSE(create({{ "Left", 1 }, { "Right", 1 }}));
    EXPECT_FALSE(create({{ "Left", std::numeric_limits<float>::quiet_NaN() }}));
    EXPECT_FALSE(create({{ "Left", 1 }, { "Right", std::numeric_limits<float>::infinity() }}));
    EXPECT_FALSE(AnimationSequence::Create("Static", Mesh->GetSkeleton(), 0, {}, {{ "Left", 0 }}));
    EXPECT_TRUE(AnimationSequence::Create("Static", Mesh->GetSkeleton(), 0, {}));
    EXPECT_TRUE(create({{ "Left", 0 }, { "Right", 3 }}));
}

TEST_F(AnimationSyncMarkerTest, AnnotationCopiesTracksAndLeavesTheOriginalSequenceUnchanged)
{
    const auto plain = MakeSequence("Imported", 8);
    const auto marked = plain->WithSyncMarkers(Target->GetSyncMarkers());
    ASSERT_TRUE(marked);
    EXPECT_NE(marked, plain);
    EXPECT_TRUE(plain->GetSyncMarkers().empty());
    EXPECT_EQ(marked->GetName(), plain->GetName());
    EXPECT_EQ(marked->GetSkeleton(), plain->GetSkeleton());
    EXPECT_FLOAT_EQ(marked->GetDuration(), plain->GetDuration());
    std::vector<SBoneTransform> originalPose, markedPose;
    ASSERT_TRUE(plain->Sample(3.5f, originalPose));
    ASSERT_TRUE(marked->Sample(3.5f, markedPose));
    EXPECT_EQ(markedPose[0].Translation, originalPose[0].Translation);
    const auto removed = marked->WithSyncMarkers({});
    ASSERT_TRUE(removed);
    EXPECT_TRUE(removed->GetSyncMarkers().empty());
    EXPECT_EQ(marked->GetSyncMarkers().size(), 2);
    EXPECT_FALSE(marked->WithSyncMarkers({{ "Invalid", 8 }}));
}

TEST_F(AnimationSyncMarkerTest, OneDimensionalBlendsKeepAStableReferenceAcrossWeightChanges)
{
    const auto space = BlendSpace1D::Create("Locomotion", {{ 1, Target }, { 0, Reference }});
    ASSERT_TRUE(space);
    std::vector<SBoneTransform> pose;
    ASSERT_TRUE(space->Sample(0.5f, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 2.5f);
    ASSERT_TRUE(space->Sample(1, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 3);
    ASSERT_TRUE(space->Sample(0, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 2);
    EXPECT_FLOAT_EQ(space->GetDuration(0.5f), 6);
    for (const float coordinate : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        ASSERT_TRUE(space->Sample(coordinate, 0.25f, pose));
        EXPECT_FLOAT_EQ(pose[0].Translation.x, 1);
    }
    SkeletalMeshInstance instance(Mesh);
    ASSERT_TRUE(instance.Play(space, { 0.5f }));
    ASSERT_TRUE(instance.Update(3));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.5f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 2.5f);
    ASSERT_TRUE(instance.SetBlendSpaceValue({ 1 }));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(instance.GetNormalizedTime(), 0.5f);
    EXPECT_FLOAT_EQ(instance.GetLocalPose()[0].Translation.x, 3);
}

TEST_F(AnimationSyncMarkerTest, TwoDimensionalBlendsSynchronizeAllThreeContributions)
{
    const auto space = BlendSpace2D::Create("Plane", {
        {{ 0, 0 }, Reference }, {{ 2, 0 }, Target }, {{ 0, 2 }, Rotated },
    });
    ASSERT_TRUE(space);
    std::vector<SBoneTransform> pose;
    ASSERT_TRUE(space->Sample({ 0.5f, 0.5f }, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 3.15625f);
    EXPECT_FLOAT_EQ(space->GetDuration({ 0.5f, 0.5f }), 5.5f);
    // Rotated is the dominant clip, while Reference still defines the shared phase.
    ASSERT_TRUE(space->Sample({ 0, 2 }, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 5.625f);
    const auto reordered = BlendSpace2D::Create("Reordered", {
        {{ 0, 2 }, Rotated }, {{ 2, 0 }, Target }, {{ 0, 0 }, Reference },
    });
    ASSERT_TRUE(reordered);
    ASSERT_TRUE(reordered->Sample({ 0.5f, 0.5f }, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 3.15625f);
}

TEST_F(AnimationSyncMarkerTest, MixedMarkedAndUnmarkedSamplesKeepTheNormalizedFallback)
{
    const auto idle = AnimationSequence::Create("Idle", Mesh->GetSkeleton(), 0, {});
    const auto plain = MakeSequence("Plain", 8);
    const auto space = BlendSpace1D::Create("Mixed", {
        { 0, idle }, { 1, Reference }, { 2, Target }, { 3, plain },
    });
    ASSERT_TRUE(space);
    std::vector<SBoneTransform> pose;
    ASSERT_TRUE(space->Sample(0, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 0);
    ASSERT_TRUE(space->Sample(2, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 3);
    ASSERT_TRUE(space->Sample(2.5f, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 3.5f);
    ASSERT_TRUE(space->Sample(3, 0.5f, pose));
    EXPECT_FLOAT_EQ(pose[0].Translation.x, 4);
    auto preserved = Mesh->GetData().ReferencePose;
    preserved[0].Translation.x = 99;
    EXPECT_FALSE(space->Sample(1, std::numeric_limits<float>::infinity(), preserved));
    EXPECT_FLOAT_EQ(preserved[0].Translation.x, 99);
}

TEST_F(AnimationSyncMarkerTest, ControllerPlaybackRetainsMarkerPhaseThroughPauseSeekAndFades)
{
    const auto space = BlendSpace1D::Create("Locomotion", {{ 0, Reference }, { 1, Target }});
    const auto controller = AnimationController::Create({
        .Name = "Controller", .InitialState = "Locomotion", .Parameters = {{ "Speed", 0.5f }},
        .States = {{ .Name = "Locomotion", .BlendSpace = space, .BlendParameters = { "Speed" } }},
    });
    ASSERT_TRUE(controller);
    SkeletalMeshInstance mesh(Mesh);
    AnimationControllerInstance instance(controller, mesh);
    ASSERT_TRUE(instance.Update(1.5f));
    EXPECT_FLOAT_EQ(mesh.GetNormalizedTime(), 0.25f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Translation.x, 1);
    mesh.Pause();
    ASSERT_TRUE(instance.Update(1));
    EXPECT_FLOAT_EQ(mesh.GetNormalizedTime(), 0.25f);
    mesh.Resume();
    ASSERT_TRUE(mesh.Seek(3));
    EXPECT_FLOAT_EQ(mesh.GetNormalizedTime(), 0.5f);
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Translation.x, 2.5f);
    ASSERT_TRUE(instance.SetFloat("Speed", 1));
    ASSERT_TRUE(instance.Update(0));
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Translation.x, 3);
    ASSERT_TRUE(mesh.Play(Reference, 1.0f));
    ASSERT_TRUE(mesh.Update(0));
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Translation.x, 3);
    ASSERT_TRUE(mesh.Update(1));
    EXPECT_FALSE(mesh.IsTransitioning());
    EXPECT_FLOAT_EQ(mesh.GetLocalPose()[0].Translation.x, 1);
}

TEST_F(AnimationSyncMarkerTest, RemappingHandlesLargeAndSmallFiniteCycleDurations)
{
    const auto maximum = std::numeric_limits<float>::max();
    const auto large = MakeSequence("Large", maximum, {
        { "LeftFootPlant", maximum * 0.125f }, { "RightFootPlant", maximum * 0.625f },
    });
    ASSERT_TRUE(large);
    const auto tinyDuration = std::numeric_limits<float>::min();
    const auto tiny = MakeSequence("Tiny", tinyDuration, {
        { "LeftFootPlant", tinyDuration * 0.125f },
        { "RightFootPlant", tinyDuration * 0.625f },
    });
    ASSERT_TRUE(tiny);
    for (const auto& clip : { large, tiny })
    {
        EXPECT_TRUE(clip->CanSynchronizeWith(*Reference));
        const auto mapped = clip->GetSynchronizedTime(0.5f, *Reference);
        EXPECT_TRUE(std::isfinite(mapped));
        EXPECT_NEAR(mapped / clip->GetDuration(), 0.375f, 1e-6f);
        EXPECT_NEAR(clip->GetSynchronizedTime(0, *Reference) / clip->GetDuration(),
            0.875f, 1e-6f);
    }
}
