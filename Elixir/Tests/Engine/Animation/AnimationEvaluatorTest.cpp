#include <Engine/Animation/AnimationEvaluator.h>
#include <Engine/Animation/AnimationPlayback.h>
#include <Engine/Animation/AnimationNotifyTracker.h>

#include <gtest/gtest.h>

#include <array>
#include <limits>

using namespace Elixir;

namespace
{
    /** Samples a resource without implementing clocks, notifications, or root motion. */
    class MinimalEvaluator final : public AnimationEvaluator
    {
    public:
        /**
         * @brief Retain a deterministic sample source.
         * @param sequence Sequence shared by independent evaluators.
         */
        explicit MinimalEvaluator(Ref<const Sequence> sequence)
            : m_Sequence(std::move(sequence)), m_Source{m_Sequence.get(), 1} {}

        /** @brief Copy sampling state without sharing a clock. */
        Scope<AnimationEvaluator> Clone() const override
        {
            return CreateScope<MinimalEvaluator>(*this);
        }

        /**
         * @brief Accept an input-free source.
         * @param values Must be empty.
         * @param parameters Unused character values.
         */
        bool Configure(
            std::span<const float> values,
            Ref<AnimationParameters> parameters
        ) override
        {
            return values.empty();
        }

        /** @brief Describe the borrowed sequence contribution. */
        SAnimationNodeState GetState() const override
        {
            return {
                .Duration = m_Sequence->GetDuration(),
                .Sources = SSequenceSources{
                    std::span(&m_Source, 1), m_Sequence.get(), nullptr
                }
            };
        }

        /**
         * @brief Sample the position already selected by playback.
         * @param context Requested phase and reference pose.
         * @param output Receives the sampled pose and source description.
         */
        bool Evaluate(
            const SAnimationEvaluationContext& context,
            SAnimationEvaluation& output
        ) override
        {
            output.Phase = context.Phase;
            output.Sources = GetState().Sources;
            return m_Sequence->Sample(
                float(context.Phase * m_Sequence->GetDuration()),
                output.Pose,
                context.ReferencePose
            );
        }

    private:
        Ref<const Sequence> m_Sequence;
        SSequenceContribution m_Source;
    };

    /** Connects the minimal evaluator to the ordinary public playback API. */
    class MinimalNode final : public AnimationNode
    {
    public:
        /**
         * @brief Retain a sequence for every evaluator created by this node.
         * @param sequence Immutable source asset.
         */
        explicit MinimalNode(Ref<const Sequence> sequence)
            : m_Sequence(std::move(sequence)) {}

        /** @brief Get the source skeleton. */
        const Ref<const Skeleton>& GetSkeleton() const override
        {
            return m_Sequence->GetSkeleton();
        }

        /** @brief Create an independent evaluator without event or movement methods. */
        Scope<AnimationEvaluator> CreateEvaluator() const override
        {
            return CreateScope<MinimalEvaluator>(m_Sequence);
        }

    private:
        Ref<const Sequence> m_Sequence;
    };
}

TEST(AnimationEvaluatorTest, PlaybackAndConsumersWorkWithOnlyTheMinimalEvaluationContract)
{
    const auto skeleton = Skeleton::Create("Root", {{.Name = "Root"}});
    const auto sequence = Sequence::Create(
        "Motion",
        skeleton,
        1,
        {{.BoneIndex = 0, .Translations = {{0, {0, 0, 0}}, {1, {4, 0, 0}}}}},
        {},
        {{"Step", 0.125f}}
    );
    ASSERT_TRUE(sequence);
    AnimationPlayback playback(CreateRef<MinimalNode>(sequence));
    ASSERT_TRUE(playback.IsValid());
    ASSERT_TRUE(playback.BindParameters(CreateRef<AnimationParameters>()));
    const SLocalPose reference{{SBoneTransform{}}};
    const auto phase = playback.Advance(0.25f, reference);
    SLocalPose sampled;
    ASSERT_TRUE(playback.Sample(phase, reference, sampled));
    EXPECT_FLOAT_EQ(sampled.LocalTransforms[0].Translation.x, 1);
    EXPECT_DOUBLE_EQ(playback.GetPhase(), 0);

    AnimationNotifyTracker tracker;
    SNotifyEvaluation notifications;
    tracker.Begin(notifications);
    ASSERT_TRUE(playback.EvaluateNotifies(0.25, "Base", tracker, notifications));
    ASSERT_EQ(notifications.Events.size(), 1);
    EXPECT_EQ(notifications.Events[0].GetName(), "Step");
    EXPECT_DOUBLE_EQ(notifications.Events[0].TimeOffset, 0.125);

    SRootMotionDelta motion;
    const SRootMotionIntervalContext interval{
        .Reference = reference.LocalTransforms[0],
        .DeltaSeconds = 0.25f
    };
    ASSERT_TRUE(playback.ExtractRootMotion(interval, motion));
    EXPECT_FLOAT_EQ(motion.Translation.x, 1);
    playback.Apply(phase);
    auto independent = playback;
    ASSERT_TRUE(independent.SeekPhase(independent.GetSeekPhase(0.75f)));
    EXPECT_FLOAT_EQ(independent.GetTime(), 0.75f);
    EXPECT_FLOAT_EQ(playback.GetTime(), 0.25f);
}

TEST(AnimationEvaluatorTest, InputBindingsKeepLocalValuesWhenNoParametersAreBound)
{
    AnimationInputBindings inputs;
    const std::array values{0.25f, 0.75f};
    ASSERT_TRUE(inputs.SetValues(values));
    ASSERT_TRUE(inputs.Bind(CreateRef<AnimationParameters>(), {}));
    ASSERT_TRUE(inputs.Refresh());
    ASSERT_EQ(inputs.GetValues().size(), 2);
    EXPECT_FLOAT_EQ(inputs.GetValues()[0], 0.25f);
    EXPECT_FLOAT_EQ(inputs.GetValues()[1], 0.75f);
}

TEST(AnimationEvaluatorTest, InvalidInputRebindingPreservesThePreviousConnection)
{
    auto parameters = CreateRef<AnimationParameters>(std::vector<SAnimationParameter>{
        {"Speed", 0.5f}, {"Attack", true}
    });
    AnimationInputBindings inputs;
    ASSERT_TRUE(inputs.Bind(parameters, std::array<size_t, 1>{0}));
    EXPECT_FALSE(inputs.Bind(parameters, std::array<size_t, 1>{1}));
    EXPECT_FALSE(inputs.Bind(parameters, std::array<size_t, 1>{99}));
    ASSERT_TRUE(parameters->SetFloat("Speed", 0.75f));
    ASSERT_TRUE(inputs.Refresh());
    ASSERT_EQ(inputs.GetValues().size(), 1);
    EXPECT_FLOAT_EQ(inputs.GetValues()[0], 0.75f);
    EXPECT_FALSE(inputs.SetValues(std::array{std::numeric_limits<float>::infinity()}));
    EXPECT_FLOAT_EQ(inputs.GetValues()[0], 0.75f);
}

TEST(AnimationEvaluatorTest, ProceduralSourcesWithoutSequenceContributionsHaveNoRootMotion)
{
    const SAnimationNodeState state{.Duration = 1, .Continuous = true};
    const SBoneTransform reference;
    const SRootMotionIntervalContext interval{.Reference = reference, .DeltaSeconds = 0.25f};
    const SAnimationMotionContext context{state, interval};
    SRootMotionDelta movement;
    movement.Translation.x = 99;
    ASSERT_TRUE(RootMotion::Extract(context, movement));
    EXPECT_EQ(movement.Translation, glm::vec3(0));
    EXPECT_EQ(movement.Rotation, glm::identity<glm::quat>());
}
