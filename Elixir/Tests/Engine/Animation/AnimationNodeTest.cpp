#include <Engine/Animation/Pose.h>
#include "AnimationTestNodes.h"
#include <gtest/gtest.h>
#include <Engine/Animation/Animation.h>
#include <Engine/Animation/StateMachine.h>
#include <Engine/Animation/BlendSpace1D.h>
#include "../Mesh/SkinningTestData.h"

using namespace Elixir;

namespace
{
    /** Records contribution resolution without depending on a renderer. */
    class CountingBlendSpace final : public BlendSpace
    {
    public:
        /**
         * @brief Retain one clip for deterministic source sampling.
         * @param clip Sequence used by every coordinate.
         */
        explicit CountingBlendSpace(Ref<const Sequence> clip)
            : BlendSpace("Counted", clip->GetSkeleton(), 1), m_Clip(std::move(clip)) {}

        mutable size_t Resolutions = 0;

    protected:
        /**
         * @brief Record one coordinate resolution and return the retained clip.
         * @param values Validated input coordinates.
         * @param output Receives one unit-weight contribution.
         */
        void FindSamples(
            std::span<const float> values,
            std::pmr::vector<SSequenceContribution>& output
        ) const override
        {
            ++Resolutions;
            output.push_back({m_Clip.get(), 1});
        }

    private:
        Ref<const Sequence> m_Clip;
    };

    /** Third-party source proving the runtime does not dispatch on known source types. */
    class DelegatingSource final : public AnimationNode
    {
    public:
        /**
         * @brief Retain a sequence used by a custom source.
         * @param clip Immutable clip retained for every independent playback.
         */
        explicit DelegatingSource(Ref<const Sequence> clip) : m_Clip(std::move(clip)) {}

        /** @brief Get the retained clip's skeleton. */
        const Ref<const Skeleton>& GetSkeleton() const override { return m_Clip->GetSkeleton(); }

        /** @brief Create independent sampling state without a new engine-side type branch. */
        Scope<AnimationNodeEvaluator> CreateEvaluator() const override
        {
            return SequenceNode::Create(m_Clip)->CreateEvaluator();
        }

    private:
        Ref<const Sequence> m_Clip;
    };
}

TEST(AnimationNodeTest, SequenceBlendSpaceMachineAndCustomSourceUseTheSamePlayEntry)
{
    const auto mesh = SkinningTests::MakeMesh();
    ASSERT_TRUE(mesh);
    const auto clip = Sequence::Create("Clip", mesh->GetSkeleton(), 1, {
        {.BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {4, 0, 0}}}}
    });
    const auto blend = BlendSpace1D::Create("Space", {{0, clip}});
    const auto machine = StateMachine::Create({
        .InitialState = "Active", .States = {{"Active", MakeNode(clip)}}
    });
    const auto custom = CreateRef<DelegatingSource>(clip);
    for (const Ref<const AnimationNode>& source :
        std::array<Ref<const AnimationNode>, 4>{MakeNode(clip), MakeNode(blend), MakeNode(machine), custom})
    {
        ASSERT_TRUE(source);
        const auto animationHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
        ASSERT_TRUE(animationHandle);
        auto& animation = *animationHandle;
        ASSERT_TRUE(animation.Play(MakeNode(source)));
        ASSERT_TRUE(animation.Update(0.25f));
        EXPECT_NEAR(animation.GetLocalPose().LocalTransforms[0].Translation.x, 1, 1e-6f);
        EXPECT_TRUE(animation.GetFrame().IsValid(mesh->GetSkeleton()->GetBones().size()));
    }
}

TEST(AnimationNodeTest, SharedMachineAssetsHaveIndependentParametersAndCommittedState)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto first = Sequence::Create("First", mesh->GetSkeleton(), 1, {});
    const auto second = first->WithNotifies({{"Selected", 0, 0, 12}});
    const auto machine = StateMachine::Create({
        .InitialState = "First", .States = {{"First", MakeNode(first)}, {"Second", MakeNode(second)}},
        .Transitions = {{.From = "First", .To = "Second",
            .Conditions = {{"Switch", EAnimationComparison::Equal, true}}, .FadeDuration = 0}}
    });
    ASSERT_TRUE(machine);
    const auto aHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(aHandle);
    auto& a = *aHandle;
    const auto bHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(bHandle);
    auto& b = *bHandle;
    ASSERT_TRUE(a.SetParameters({{"Switch", false}}));
    ASSERT_TRUE(b.SetParameters({{"Switch", false}}));
    ASSERT_TRUE(a.Play(MakeNode(machine)));
    ASSERT_TRUE(b.Play(MakeNode(machine)));
    ASSERT_TRUE(a.GetParameters().SetBool("Switch", true));
    ASSERT_TRUE(a.Update(0.25f));
    ASSERT_TRUE(b.Update(0.25f));
    EXPECT_EQ(a.GetCurrentState(), "Second");
    EXPECT_EQ(b.GetCurrentState(), "First");
    ASSERT_EQ(a.GetNotifyEvents().size(), 1);
    EXPECT_EQ(a.GetNotifyEvents()[0].GetName(), "Selected");
    EXPECT_TRUE(b.GetNotifyEvents().empty());
}

TEST(AnimationNodeTest, BoundBlendSpaceResolvesOnceUntilItsInputsChange)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto clip = Sequence::Create("Clip", mesh->GetSkeleton(), 1, {});
    const auto space = CreateRef<CountingBlendSpace>(clip);
    const std::array initial{0.0f};
    AnimationPlayback playback(MakeNode(space), initial);
    ASSERT_TRUE(playback.IsValid());
    EXPECT_EQ(space->Resolutions, 1);
    for (size_t frame = 0; frame < 10; ++frame)
    {
        const auto phase = playback.Advance(0.05f, mesh->GetData().ReferencePose);
        SLocalPose pose;
        ASSERT_TRUE(playback.Sample(phase, mesh->GetData().ReferencePose, pose));
        EXPECT_TRUE(playback.GetTraversal(0.05f).IsValid());
        SRootMotionDelta movement;
        const SRootMotionIntervalContext interval{
            .Reference = mesh->GetData().ReferencePose.LocalTransforms[0],
            .DeltaSeconds = 0.05f
        };
        ASSERT_TRUE(playback.ExtractRootMotion(interval, movement));
        playback.Apply(phase);
    }
    EXPECT_EQ(space->Resolutions, 1);
    ASSERT_TRUE(playback.SetInputs(initial));
    EXPECT_EQ(space->Resolutions, 1);
    const std::array changed{1.0f};
    ASSERT_TRUE(playback.SetInputs(changed));
    EXPECT_EQ(space->Resolutions, 2);
}

TEST(AnimationNodeTest, CompositeSourcesConsumeInitialNotifiesOnlyAfterPositivePublication)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto clip = Sequence::Create("Clip", mesh->GetSkeleton(), 1, {}, {}, {
        {"Boundary", 0.25f}
    });
    const auto machine = StateMachine::Create({
        .InitialState = "Active", .States = {{"Active", MakeNode(clip)}}
    });
    const auto animationHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(animationHandle);
    auto& animation = *animationHandle;
    ASSERT_TRUE(animation.Play(MakeNode(machine)));
    ASSERT_TRUE(animation.Update(0.25f));
    ASSERT_EQ(animation.GetNotifyEvents().size(), 1);
    ASSERT_TRUE(animation.Update(0.25f));
    EXPECT_TRUE(animation.GetNotifyEvents().empty());
    ASSERT_TRUE(animation.Update(0));
    EXPECT_TRUE(animation.GetNotifyEvents().empty());
}

TEST(AnimationNodeTest, DefinitionsValidateTheirOwnContextWithoutMutatingIt)
{
    EXPECT_TRUE((SAnimationNotify{"Instant", 1}.IsValid(1)));
    EXPECT_TRUE((SAnimationNotify{"Window", 0.25f, 0.75f}.IsValid(1)));
    EXPECT_FALSE((SAnimationNotify{"PastEnd", 0.5f, 0.75f}.IsValid(1)));
    EXPECT_FALSE((SAnimationNotify{"Negative", 0, -1}.IsValid(1)));
    AnimationParameters parameters({{"Speed", 1.0f}, {"Grounded", true}});
    EXPECT_TRUE(parameters.IsValid());
    EXPECT_FALSE(parameters.SetFloat("Grounded", 2));
    EXPECT_EQ(parameters.GetBool("Grounded"), true);
    EXPECT_FALSE((SAnimationCondition{"Grounded", EAnimationComparison::Greater, true}
        .IsValid(parameters)));
    const SAnimationCondition moving{"Speed", EAnimationComparison::Greater, 0.0f};
    const auto speed = parameters.Find("Speed");
    ASSERT_TRUE(speed);
    EXPECT_TRUE(parameters.Matches(*speed, moving));
    EXPECT_FALSE(parameters.Matches(100, moving));
    EXPECT_FALSE(parameters.Matches(*parameters.Find("Grounded"), moving));
    EXPECT_FALSE(parameters.Matches(*parameters.Find("Grounded"),
        {"Grounded", EAnimationComparison::Less, false}));
}

TEST(AnimationNodeTest, SeekingAMachineUsesItsNonloopingChildPolicy)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto clip = Sequence::Create("Clip", mesh->GetSkeleton(), 1, {
        {.BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {4, 0, 0}}}}
    });
    const auto machine = StateMachine::Create({
        .InitialState = "Active", .States = {{.Name = "Active", .Node = MakeNode(clip), .Looping = false}}
    });
    const auto animationHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(animationHandle);
    auto& animation = *animationHandle;
    ASSERT_TRUE(animation.Play(MakeNode(machine)));
    ASSERT_TRUE(animation.Seek(2.5f));
    EXPECT_FLOAT_EQ(animation.GetNormalizedTime(), 1);
    EXPECT_FLOAT_EQ(animation.GetLocalPose().LocalTransforms[0].Translation.x, 4);
    ASSERT_TRUE(animation.Update(0.25f));
    EXPECT_FLOAT_EQ(animation.GetTime(), 1);
}

TEST(AnimationNodeTest, NestedMachineTransitionsRestartSameAssetNotifyActivations)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto clip = Sequence::Create("Clip", mesh->GetSkeleton(), 1, {}, {}, {
        {"Window", 0, 1}
    });
    const auto inner = StateMachine::Create({
        .InitialState = "First", .States = {{"First", MakeNode(clip)}, {"Second", MakeNode(clip)}},
        .Transitions = {{.From = "First", .To = "Second",
            .Conditions = {{"Switch", EAnimationComparison::Equal, true}}, .FadeDuration = 0}}
    });
    const auto outer = StateMachine::Create({
        .InitialState = "Outer", .States = {{"Outer", MakeNode(inner)}}
    });
    ASSERT_TRUE(outer);
    const auto animationHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(animationHandle);
    auto& animation = *animationHandle;
    ASSERT_TRUE(animation.SetParameters({{"Switch", false}}));
    ASSERT_TRUE(animation.Play(MakeNode(outer)));
    ASSERT_TRUE(animation.Update(0.25f));
    ASSERT_EQ(animation.GetNotifyEvents().size(), 2);
    const auto activation = animation.GetNotifyEvents()[0].ActivationId;
    ASSERT_TRUE(animation.GetParameters().SetBool("Switch", true));
    ASSERT_TRUE(animation.Update(0.1f));
    const auto events = animation.GetNotifyEvents();
    ASSERT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].Type, EAnimationNotifyEventType::End);
    EXPECT_TRUE(events[0].Cancelled);
    EXPECT_EQ(events[0].ActivationId, activation);
    EXPECT_EQ(events[1].Type, EAnimationNotifyEventType::Begin);
    EXPECT_NE(events[1].ActivationId, activation);
}

TEST(AnimationNodeTest, GlobalPauseFreezesMachineDecisionsUntilResume)
{
    const auto mesh = SkinningTests::MakeMesh();
    const auto first = Sequence::Create("First", mesh->GetSkeleton(), 1, {});
    const auto second = Sequence::Create("Second", mesh->GetSkeleton(), 1, {});
    const auto machine = StateMachine::Create({
        .InitialState = "First", .States = {{"First", MakeNode(first)}, {"Second", MakeNode(second)}},
        .Transitions = {{.From = "First", .To = "Second",
            .Conditions = {{"Switch", EAnimationComparison::Equal, true}}, .FadeDuration = 0}}
    });
    const auto animationHandle = Animation::Create(mesh->GetSkeleton(), mesh->GetData().ReferencePose);
    ASSERT_TRUE(animationHandle);
    auto& animation = *animationHandle;
    ASSERT_TRUE(animation.SetParameters({{"Switch", false}}));
    ASSERT_TRUE(animation.Play(MakeNode(machine)));
    ASSERT_TRUE(animation.Update(0.25f));
    animation.Pause();
    ASSERT_TRUE(animation.GetParameters().SetBool("Switch", true));
    ASSERT_TRUE(animation.Update(1));
    EXPECT_EQ(animation.GetCurrentState(), "First");
    EXPECT_FLOAT_EQ(animation.GetTime(), 0.25f);
    animation.Resume();
    ASSERT_TRUE(animation.Update(0));
    EXPECT_EQ(animation.GetCurrentState(), "Second");
}
