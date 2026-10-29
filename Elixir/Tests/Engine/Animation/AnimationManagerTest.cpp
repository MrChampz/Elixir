#include "AnimationInertializationTestFixture.h"
#include "AnimationTestNodes.h"

#include <Engine/Animation/AnimationManager.h>
#include <Engine/Core/Timer.h>
#include <Engine/Mesh/SkeletalMeshRenderSnapshot.h>
#include <Engine/Animation/AnimationGraph.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <functional>
#include <mutex>
#include <thread>

using namespace Elixir;
using namespace Elixir::Tests::Inertia;
using AnimationManagerTest = AnimationInertializationFixture;

namespace
{
    /** Runs a test callback inside pose evaluation. */
    class CallbackModifier final : public PoseModifier
    {
    public:
        /**
         * @brief Retain a callback for each pose evaluation.
         * @param callback Action run on the evaluating thread.
         */
        explicit CallbackModifier(std::function<void()> callback)
            : m_Callback(std::move(callback)) {}

        /**
         * @brief Invoke the callback without changing the candidate pose.
         * @param context Unused rig context.
         * @param pose Unchanged candidate pose.
         */
        bool Apply(const SPoseModifierContext& context, SLocalPose& pose) const override
        {
            m_Callback();
            return true;
        }

    private:
        std::function<void()> m_Callback;
    };

    /** Records overlapping modifier calls without blocking a failed test indefinitely. */
    class ConcurrentModifier final : public PoseModifier
    {
    public:
        /** @brief Remember the thread that configures the modifier. */
        ConcurrentModifier() : m_Owner(std::this_thread::get_id()) {}

        /** @brief Enable the probe after initial pose configuration finishes. */
        void Enable() { m_Enabled = true; }

        /**
         * @brief Wait for another worker to reach the same evaluation point.
         * @param context Unused rig context.
         * @param pose Unchanged candidate pose.
         */
        bool Apply(const SPoseModifierContext& context, SLocalPose& pose) const override
        {
            if (!m_Enabled)
                return true;

            std::unique_lock lock(m_Mutex);
            if (std::this_thread::get_id() == m_Owner)
                return false;

            ++m_Arrivals;
            m_Condition.notify_all();
            const auto bothArrived = m_Condition.wait_for(
                lock,
                std::chrono::seconds(2),
                [this] { return m_Arrivals == 2; }
            );
            return bothArrived;
        }

    private:
        std::thread::id m_Owner;
        bool m_Enabled = false;
        mutable std::mutex m_Mutex;
        mutable std::condition_variable m_Condition;
        mutable size_t m_Arrivals = 0;
    };
}

TEST_F(AnimationManagerTest, EvaluatesIndependentPosesOnOverlappingWorkerThreads)
{
    // Executor reserves two hardware threads and always creates at least one worker.
    if (std::thread::hardware_concurrency() <= 3)
        GTEST_SKIP() << "Executor has fewer than two worker threads.";

    const auto other = CreatePlayback();
    ASSERT_TRUE(other);
    const auto modifier = CreateRef<ConcurrentModifier>();
    const std::array<Ref<const PoseModifier>, 1> modifiers{modifier};
    ASSERT_TRUE(Playback->SetPoseModifiers(modifiers));
    ASSERT_TRUE(other->SetPoseModifiers(modifiers));
    modifier->Enable();

    EXPECT_TRUE(AnimationManager::Get().Update(Timestep(0.1f)));
}

TEST_F(AnimationManagerTest, SharedGraphMatchesSequentialPosesCurvesRootMotionAndEvents)
{
    const auto source = Moving->WithNotifies({{"Start", 0}, {"Step", 0.05f}});
    const auto machine = StateMachine::Create(
        {.InitialState = "Moving",
         .States =
             {{.Name = "Moving", .Node = MakeNode(source)},
              {.Name = "Target", .Node = MakeNode(Target)}},
         .Transitions = {
             {.From = "Moving",
              .To = "Target",
              .Conditions = {{"Change", EAnimationComparison::Equal, true}},
              .FadeDuration = 0.15f,
              .Mode = Inertialization}
         }}
    );
    ASSERT_TRUE(machine);
    const auto node = MakeNode(machine);
    const auto mask = BoneMask::Create("Child", Mesh->GetSkeleton(), {0, 1});
    ASSERT_TRUE(mask);
    std::vector<Ref<Animation>> serial, parallel;
    for (size_t i = 0; i < 8; ++i)
    {
        serial.push_back(CreatePlayback());
        ASSERT_TRUE(serial.back());
        serial.back()->SetManaged(false);
        parallel.push_back(CreatePlayback());
        for (const auto& instance : {serial.back(), parallel.back()})
        {
            ASSERT_TRUE(instance);
            ASSERT_TRUE(instance->SetParameters({{"Change", false}}));
            ASSERT_TRUE(instance->SetRootMotionEnabled(true));
            ASSERT_TRUE(instance->Play(node));
            ASSERT_TRUE(instance->AddLayer("Child", mask, 0.25f));
            ASSERT_TRUE(instance->GetLayer("Child")->GetPlayer().Play(MakeNode(source)));
        }
    }

    for (size_t frame = 0; frame < 30; ++frame)
    {
        for (size_t i = 0; i < serial.size(); ++i)
        {
            for (const auto& instance : {serial[i], parallel[i]})
            {
                if (frame == 5 + i)
                    ASSERT_TRUE(instance->GetParameters().SetBool("Change", true));
                if (frame == 18)
                    instance->Pause();
                if (frame == 20)
                    instance->Resume();
                if (frame == 22)
                    ASSERT_TRUE(instance->Seek(0.05f));
                if (frame == 24)
                    ASSERT_TRUE(instance->Play(MakeNode(source), 0.1f, true, Inertialization));
            }
            ASSERT_TRUE(serial[i]->Update(0.016f));
        }
        ASSERT_TRUE(AnimationManager::Get().Update(Timestep(0.016f)));

        for (size_t i = 0; i < serial.size(); ++i)
        {
            SCOPED_TRACE(frame);
            SCOPED_TRACE(i);
            const auto& expected = *serial[i];
            const auto& actual = *parallel[i];
            EXPECT_EQ(actual.GetModelPose().Matrices, expected.GetModelPose().Matrices);
            EXPECT_EQ(
                actual.GetCurveValues().GetValues(),
                expected.GetCurveValues().GetValues()
            );
            EXPECT_EQ(
                actual.GetRootMotionDelta().Translation,
                expected.GetRootMotionDelta().Translation
            );
            EXPECT_EQ(
                actual.GetRootMotionDelta().Rotation,
                expected.GetRootMotionDelta().Rotation
            );
            EXPECT_EQ(actual.GetCurrentState(), expected.GetCurrentState());
            EXPECT_FLOAT_EQ(actual.GetTime(), expected.GetTime());
            const auto& actualEvents = actual.GetNotifyEvents();
            const auto& expectedEvents = expected.GetNotifyEvents();
            ASSERT_EQ(actualEvents.size(), expectedEvents.size());
            for (size_t event = 0; event < actualEvents.size(); ++event)
            {
                EXPECT_EQ(actualEvents[event].GetName(), expectedEvents[event].GetName());
                EXPECT_EQ(actualEvents[event].Type, expectedEvents[event].Type);
                EXPECT_EQ(actualEvents[event].Cancelled, expectedEvents[event].Cancelled);
                EXPECT_EQ(actualEvents[event].TimeOffset, expectedEvents[event].TimeOffset);
            }
        }
    }
}

TEST_F(AnimationManagerTest, RejectsInvalidDurationBeforeStartingAnyInstance)
{
    ASSERT_TRUE(Playback->Play(MakeNode(Moving)));
    EXPECT_FALSE(AnimationManager::Get().Update(Timestep(-1)));
    EXPECT_FALSE(AnimationManager::Get().Update(
        Timestep(std::numeric_limits<float>::quiet_NaN())
    ));
    EXPECT_FALSE(AnimationManager::Get().Update(
        Timestep(std::numeric_limits<float>::infinity())
    ));
    EXPECT_FLOAT_EQ(Playback->GetTime(), 0);
    EXPECT_TRUE(AnimationManager::Get().Update(Timestep(0)));
    EXPECT_FLOAT_EQ(Playback->GetTime(), 0);
}

TEST_F(AnimationManagerTest, FactoryRegistersInstancesAndManualPlaybackCanOptOut)
{
    const auto other = CreatePlayback();
    ASSERT_TRUE(other);
    ASSERT_TRUE(Playback->Play(MakeNode(Moving)));
    ASSERT_TRUE(other->Play(MakeNode(Moving)));
    EXPECT_TRUE(Playback->IsManaged());
    other->SetManaged(false);
    ASSERT_TRUE(AnimationManager::Get().Update(Timestep(0.1f)));
    EXPECT_FLOAT_EQ(Playback->GetTime(), 0.1f);
    EXPECT_FLOAT_EQ(other->GetTime(), 0);
    other->SetManaged(true);
    ASSERT_TRUE(AnimationManager::Get().Update(Timestep(0.1f)));
    EXPECT_FLOAT_EQ(Playback->GetTime(), 0.2f);
    EXPECT_FLOAT_EQ(other->GetTime(), 0.1f);
}

TEST_F(AnimationManagerTest, RegistryDoesNotRetainDestroyedInstances)
{
    auto transient = CreatePlayback();
    ASSERT_TRUE(transient);
    const std::weak_ptr<Animation> observed = transient;
    transient.reset();
    EXPECT_TRUE(observed.expired());
    ASSERT_TRUE(AnimationManager::Get().Update(Timestep(0.1f)));
    EXPECT_TRUE(observed.expired());
}

TEST_F(AnimationManagerTest, FailedInstancePreservesItsFrameAndOtherJobsStillFinish)
{
    const auto other = CreatePlayback();
    ASSERT_TRUE(other);
    ASSERT_TRUE(Playback->Play(MakeNode(Moving)));
    ASSERT_TRUE(other->Play(MakeNode(Moving)));
    const auto mask = BoneMask::Create("All", Mesh->GetSkeleton(), {1, 1});
    const auto reflection = Sequence::Create(
        "Reflection",
        Mesh->GetSkeleton(),
        5,
        {{.BoneIndex = 0, .Scales = {{0, {-1, -1, -1}}}}}
    );
    ASSERT_TRUE(mask);
    ASSERT_TRUE(reflection);
    ASSERT_TRUE(Playback->AddLayer("Invalid", mask, 0.5f));
    ASSERT_TRUE(Playback->GetLayer("Invalid")->GetPlayer().Play(MakeNode(reflection)));
    const auto previousFrame = Playback->GetFrameSnapshot();
    const auto previousPose = Playback->GetModelPose().Matrices;
    EXPECT_FALSE(AnimationManager::Get().Update(Timestep(0.1f)));
    EXPECT_EQ(Playback->GetFrameSnapshot(), previousFrame);
    EXPECT_EQ(Playback->GetModelPose().Matrices, previousPose);
    EXPECT_FLOAT_EQ(Playback->GetTime(), 0);
    EXPECT_FLOAT_EQ(other->GetTime(), 0.1f);
    EXPECT_NEAR(other->GetLocalPose().LocalTransforms[0].Translation.x, 0.2f, 1e-6f);
}

TEST_F(AnimationManagerTest, GraphInstancesJoinAutomaticallyAndAdvanceExactlyOnce)
{
    const auto graph = AnimationGraph::Create(
        {.Name = "Automatic", .Output = MakeNode(Moving)}
    );
    ASSERT_TRUE(graph);
    const auto first = graph->CreateAnimation(Mesh->GetData().ReferencePose);
    const auto second = graph->CreateAnimation(Mesh->GetData().ReferencePose);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(AnimationManager::Get().Update(Timestep(0.125f)));
    EXPECT_FLOAT_EQ(first->GetTime(), 0.125f);
    EXPECT_FLOAT_EQ(second->GetTime(), 0.125f);
}

TEST_F(AnimationManagerTest, InstancesCreatedDuringEvaluationJoinTheNextFrame)
{
    Ref<Animation> created;
    bool enabled = false;
    const auto callback = [&]
    {
        if (!enabled || created)
            return;
        created = CreatePlayback();
        if (created)
            created->Play(MakeNode(Moving));
    };
    const auto modifier = CreateRef<CallbackModifier>(callback);
    const std::array<Ref<const PoseModifier>, 1> modifiers{modifier};
    ASSERT_TRUE(Playback->SetPoseModifiers(modifiers));
    enabled = true;
    ASSERT_TRUE(AnimationManager::Get().Update(Timestep(0.1f)));
    ASSERT_TRUE(created);
    EXPECT_FLOAT_EQ(created->GetTime(), 0);
    ASSERT_TRUE(AnimationManager::Get().Update(Timestep(0.1f)));
    EXPECT_FLOAT_EQ(created->GetTime(), 0.1f);
}

TEST_F(AnimationManagerTest, NestedUpdatesAreRejectedWithoutWaitingForTheirOwnWorkers)
{
    bool enabled = false;
    bool nestedSucceeded = true;
    const auto callback = [&]
    {
        if (enabled)
            nestedSucceeded = AnimationManager::Get().Update(Timestep(0.1f));
    };
    const auto modifier = CreateRef<CallbackModifier>(callback);
    const std::array<Ref<const PoseModifier>, 1> modifiers{modifier};
    ASSERT_TRUE(Playback->SetPoseModifiers(modifiers));
    enabled = true;
    ASSERT_TRUE(AnimationManager::Get().Update(Timestep(0.1f)));
    EXPECT_FALSE(nestedSucceeded);
}

TEST_F(AnimationManagerTest, RenderSnapshotsRetainMeshesAndDoNotChangeWithLaterFrames)
{
    auto instance = SkeletalMeshInstance::Create(Mesh);
    ASSERT_TRUE(instance);
    ASSERT_TRUE(Playback->Play(MakeNode(Moving)));
    const std::weak_ptr<SkeletalMeshInstance> observed = instance;
    SSkeletalMeshRenderSnapshot first{instance, Playback->GetFrameSnapshot(), glm::mat4(1)};
    const auto previousPose = first.Frame->ModelPose.Matrices;
    instance.reset();
    EXPECT_FALSE(observed.expired());
    ASSERT_TRUE(AnimationManager::Get().Update(Timestep(0.1f)));
    EXPECT_EQ(first.Frame->ModelPose.Matrices, previousPose);
    EXPECT_NE(Playback->GetModelPose().Matrices, previousPose);
    first.Instance.reset();
    EXPECT_TRUE(observed.expired());
}

TEST_F(AnimationManagerTest, PublishedFramesSurvivePlaybackCommandsAndInstanceDestruction)
{
    auto animation = CreatePlayback();
    ASSERT_TRUE(animation);
    const auto source = Moving->WithNotifies({{"Start", 0}, {"Step", 0.05f}});
    ASSERT_TRUE(animation->SetRootMotionEnabled(true));
    ASSERT_TRUE(animation->Play(MakeNode(source)));
    ASSERT_TRUE(animation->Update(0.1f));
    const auto retained = animation->GetFrameSnapshot();
    ASSERT_TRUE(retained);
    EXPECT_EQ(animation->GetFrameSnapshot(), retained);
    const auto expected = *retained;
    const auto verifyRetained = [&]
    {
        EXPECT_EQ(retained->ModelPose.Matrices, expected.ModelPose.Matrices);
        EXPECT_EQ(retained->Sample.Curves.GetValues(), expected.Sample.Curves.GetValues());
        ASSERT_EQ(
            retained->Sample.Pose.LocalTransforms.size(),
            expected.Sample.Pose.LocalTransforms.size()
        );
        for (size_t i = 0; i < expected.Sample.Pose.LocalTransforms.size(); ++i)
        {
            const auto& actual = retained->Sample.Pose.LocalTransforms[i];
            const auto& original = expected.Sample.Pose.LocalTransforms[i];
            EXPECT_EQ(actual.Translation, original.Translation);
            EXPECT_EQ(actual.Rotation, original.Rotation);
            EXPECT_EQ(actual.Scale, original.Scale);
        }
        EXPECT_EQ(retained->RootMotion.Translation, expected.RootMotion.Translation);
        EXPECT_EQ(retained->RootMotion.Rotation, expected.RootMotion.Rotation);
        ASSERT_EQ(retained->NotifyEvents.size(), expected.NotifyEvents.size());
        for (size_t i = 0; i < expected.NotifyEvents.size(); ++i)
        {
            EXPECT_EQ(retained->NotifyEvents[i].GetName(), expected.NotifyEvents[i].GetName());
            EXPECT_EQ(retained->NotifyEvents[i].Type, expected.NotifyEvents[i].Type);
        }
    };
    ASSERT_TRUE(animation->Update(0.2f));
    EXPECT_NE(animation->GetFrameSnapshot(), retained);
    verifyRetained();
    ASSERT_TRUE(animation->Seek(0.4f));
    verifyRetained();
    ASSERT_TRUE(animation->RefreshPose());
    verifyRetained();
    ASSERT_TRUE(animation->SetRootMotionEnabled(false));
    verifyRetained();
    animation->Stop();
    verifyRetained();
    ASSERT_TRUE(animation->Play(MakeNode(source)));
    verifyRetained();
    ASSERT_TRUE(animation->SetLocalPose(Mesh->GetData().ReferencePose));
    verifyRetained();
    animation.reset();
    verifyRetained();
}
