#include "Animation/AnimationGraphDocument.h"

#include <Engine/Animation/SequenceNode.h>
#include <gtest/gtest.h>

#include <sstream>

using namespace Elixir;

namespace
{
    class AnimationGraphDocumentTest : public testing::Test
    {
      protected:
        void SetUp() override
        {
            if (!Log::GetCoreLogger())
                Log::Init();
            skeleton = Skeleton::Create("Test", {{"Root"}});
            source = SequenceNode::Create(Sequence::Create("Idle", skeleton, 1, {}));
            sources.emplace("Idle Clip", source);
            document.Name = "Graph with spaces";
            document.Parameters = {{"Speed", 0.0f}, {"Grounded", true}};
            document.Machine.InitialState = "Idle";
            document.Machine.States = {{"Idle", source}, {"Walk", source}};
            document.Machine.Transitions = {
                {"Idle", "Walk", {{"Speed", EAnimationComparison::Greater, 1.0f}}}
            };
            document.Positions = {{-12.5f, 100}, {220, 150}};
        }

        Ref<Skeleton> skeleton;
        Ref<SequenceNode> source;
        AnimationGraphDocument::Sources sources;
        AnimationGraphDocument document;
    };
}

TEST_F(AnimationGraphDocumentTest, RenameAndDeleteKeepReferencesConsistent)
{
    EXPECT_FALSE(document.RenameState(0, "Walk"));
    EXPECT_FALSE(document.RenameState(0, ""));
    ASSERT_TRUE(document.RenameState(0, "Standing"));
    EXPECT_EQ(document.Machine.InitialState, "Standing");
    EXPECT_EQ(document.Machine.Transitions[0].From, "Standing");
    document.RemoveState(0);
    EXPECT_EQ(document.Machine.InitialState, "Walk");
    EXPECT_TRUE(document.Machine.Transitions.empty());
    ASSERT_EQ(document.Positions.size(), 1);
    EXPECT_EQ(document.Positions[0], glm::vec2(220, 150));
    EXPECT_TRUE(document.Validate().empty());
}

TEST_F(AnimationGraphDocumentTest, RoundTripPreservesTypesPositionsAndPriority)
{
    document.Machine.Transitions.push_back({"Walk", "Idle", {}, 0.6f, false});
    std::stringstream file;
    ASSERT_TRUE(document.Write(file, sources));
    AnimationGraphDocument loaded;
    ASSERT_TRUE(loaded.Read(file, sources));
    EXPECT_EQ(loaded.Name, document.Name);
    EXPECT_EQ(loaded.Positions, document.Positions);
    EXPECT_EQ(loaded.Parameters[0].DefaultValue, TAnimationParameterValue(0.0f));
    EXPECT_EQ(loaded.Parameters[1].DefaultValue, TAnimationParameterValue(true));
    ASSERT_EQ(loaded.Machine.Transitions.size(), 2);
    EXPECT_EQ(loaded.Machine.Transitions[0].To, "Walk");
    EXPECT_FLOAT_EQ(loaded.Machine.Transitions[1].FadeDuration, 0.6f);
    EXPECT_EQ(loaded.Machine.States[0].Node, source);
}

TEST_F(AnimationGraphDocumentTest, RejectsMalformedFilesWithoutReplacingDocument)
{
    for (const auto& text :
         {"ELIXIR_ANIMATION_GRAPH 99",
          "ELIXIR_ANIMATION_GRAPH 1\n\"Other\" \"Idle\"\n9999999999",
          "ELIXIR_ANIMATION_GRAPH 1\n\"Other\" \"Idle\"\n1\n\"Flag\" 1 2"})
    {
        std::istringstream input(text);
        EXPECT_FALSE(document.Read(input, sources));
        EXPECT_EQ(document.Name, "Graph with spaces");
    }
    std::stringstream file;
    ASSERT_TRUE(document.Write(file, sources));
    EXPECT_FALSE(document.Read(file, {}));
    EXPECT_EQ(document.Name, "Graph with spaces");
}

TEST_F(AnimationGraphDocumentTest, ValidatesBindingsAndTypedConditions)
{
    document.Machine.Transitions[0].Conditions[0].Value = true;
    EXPECT_FALSE(document.Validate().empty());
    EXPECT_EQ(document.Compile(), nullptr);
    document.Machine.Transitions[0].Conditions[0].Value = 1.0f;
    document.Machine.States[0].InputParameters.push_back("Missing");
    EXPECT_FALSE(document.Validate().empty());
}

TEST_F(AnimationGraphDocumentTest, CompilesIndependentPreviewsThatEvaluateAuthoredTransitions)
{
    const auto graph = document.Compile();
    ASSERT_NE(graph, nullptr);
    SLocalPose reference;
    reference.LocalTransforms.resize(1);
    const auto first = graph->CreateAnimation(reference);
    const auto second = graph->CreateAnimation(reference);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_TRUE(first->GetParameters().SetFloat("Speed", 2));
    ASSERT_TRUE(first->Update(0.25f));
    ASSERT_TRUE(second->Update(0.25f));
    EXPECT_EQ(first->GetCurrentState(), "Walk");
    EXPECT_EQ(second->GetCurrentState(), "Idle");
}

namespace
{
    /** @brief Create complete editable resources with distinguishable sampled poses. */
    AnimationGraphDocument CreateResourceGraph()
    {
        if (!Log::GetCoreLogger())
            Log::Init();
        AnimationGraphDocument document;
        const auto skeleton = Skeleton::Create("Editable", {{"Root"}});
        for (size_t i = 0; i < 3; ++i)
        {
            SAnimationSequenceAsset clip;
            clip.Name = "Clip" + std::to_string(i);
            clip.Skeleton = skeleton;
            SBoneAnimationTrack track;
            track.Translations = {{0, {float(i), 0, 0}}, {1, {float(i), 0, 0}}};
            track.Rotations = {{0, {1, 0, 0, 0}}, {1, {1, 0, 0, 0}}};
            track.Scales = {{0, {1, 1, 1}}, {1, {1, 1, 1}}};
            clip.Tracks = {track};
            clip.Markers = {{"Left", 0}, {"Right", 0.5f}};
            clip.Notifies = {{.Name = "Step", .Time = 0.25f, .Duration = 0.1f}};
            document.Library.Sequences.push_back(clip);
        }
        document.Library.BlendSpaces = {
            {"SpeedBlend",
             false,
             "Speed",
             "Direction",
             {{"Clip0", {0, 0}}, {"Clip2", {2, 0}}}},
            {"PlaneBlend",
             true,
             "Speed",
             "Direction",
             {{"Clip0", {0, 0}}, {"Clip1", {1, 0}}, {"Clip2", {0, 1}}}}
        };
        document.Library.Montages = {
            {"Action", "Clip2", 0, 0, true, {{"Start", 0}, {"Finish", 0.5f}}}
        };
        document.Parameters = {{"Speed", 0.0f}, {"Direction", 0.0f}};
        document.OutputSource = "SpeedBlend";
        return document;
    }
}

TEST(AnimationGraphResourcesTest, CompleteLibraryRoundTripsWithoutExternalSources)
{
    auto document = CreateResourceGraph();
    document.Library.Sequences[1].Tracks[0].TranslationInterpolation =
        EBoneInterpolation::Step;
    document.Library.Montages[0].Sections[1].NextSection = "";
    std::stringstream file;
    ASSERT_TRUE(document.Write(file, {}));
    AnimationGraphDocument loaded;
    ASSERT_TRUE(loaded.Read(file, {}));
    ASSERT_EQ(loaded.Library.Sequences.size(), 3);
    ASSERT_EQ(loaded.Library.BlendSpaces.size(), 2);
    ASSERT_EQ(loaded.Library.Montages.size(), 1);
    EXPECT_EQ(loaded.OutputSource, "SpeedBlend");
    EXPECT_EQ(
        loaded.Library.Sequences[1].Tracks[0].TranslationInterpolation,
        EBoneInterpolation::Step
    );
    EXPECT_EQ(loaded.Library.Sequences[0].Markers[1].Name, "Right");
    EXPECT_FLOAT_EQ(loaded.Library.Sequences[0].Notifies[0].Duration, 0.1f);
    EXPECT_EQ(
        loaded.Library.Montages[0].Sections[1].NextSection,
        std::optional<std::string>("")
    );
    ASSERT_NE(loaded.Compile(), nullptr);
}

TEST(AnimationGraphResourcesTest, BlendOutputsEvaluateRealWeightsAndPoseChanges)
{
    auto document = CreateResourceGraph();
    SLocalPose reference;
    reference.LocalTransforms.resize(1);
    auto graph = document.Compile();
    ASSERT_TRUE(graph);
    auto preview = graph->CreateAnimation(reference);
    ASSERT_TRUE(preview);
    ASSERT_TRUE(preview->GetParameters().SetFloat("Speed", 1));
    ASSERT_TRUE(preview->Update(0.1f));
    EXPECT_NEAR(preview->GetLocalPose().LocalTransforms[0].Translation.x, 1, 0.001f);
    document.OutputSource = "PlaneBlend";
    graph = document.Compile();
    ASSERT_TRUE(graph);
    preview = graph->CreateAnimation(reference);
    ASSERT_TRUE(preview);
    ASSERT_TRUE(preview->GetParameters().SetFloat("Direction", 1));
    ASSERT_TRUE(preview->Update(0.1f));
    EXPECT_NEAR(preview->GetLocalPose().LocalTransforms[0].Translation.x, 2, 0.001f);
}

TEST(AnimationGraphResourcesTest, MontageSectionsPlayOverContinuingGraphAndReturnToItsPose)
{
    auto document = CreateResourceGraph();
    document.OutputSource = "Clip0";
    const auto graph = document.Compile();
    ASSERT_TRUE(graph);
    SLocalPose reference;
    reference.LocalTransforms.resize(1);
    const auto preview = graph->CreateAnimation(reference);
    ASSERT_TRUE(preview);
    const auto montage = document.Library.BuildMontage(0);
    ASSERT_TRUE(montage);
    auto& player = preview->GetBaseLayer().GetPlayer();
    ASSERT_TRUE(player.PlayMontage(montage));
    ASSERT_TRUE(preview->Update(0.1f));
    EXPECT_NEAR(preview->GetLocalPose().LocalTransforms[0].Translation.x, 2, 0.001f);
    ASSERT_TRUE(player.JumpToMontageSection("Finish"));
    EXPECT_EQ(player.GetCurrentMontageSection(), "Finish");
    ASSERT_TRUE(preview->Update(0.6f));
    EXPECT_NEAR(preview->GetLocalPose().LocalTransforms[0].Translation.x, 0, 0.001f);
}

TEST(
    AnimationGraphResourcesTest,
    RenamedSequencesUpdateConsumersAndInvalidLibrariesAreRejected
)
{
    auto document = CreateResourceGraph();
    ASSERT_TRUE(document.Library.Rename(EAnimationAssetKind::Sequence, 2, "Running"));
    EXPECT_EQ(document.Library.BlendSpaces[0].Samples[1].Sequence, "Running");
    EXPECT_EQ(document.Library.Montages[0].Sequence, "Running");
    EXPECT_TRUE(document.Validate().empty());
    document.Library.BlendSpaces[1].Samples[2].Position = {0, 0};
    EXPECT_FALSE(document.Validate().empty());
    EXPECT_FALSE(document.Compile());
}

TEST(AnimationGraphResourcesTest, StateConnectionsResolveEditedResourcesInsteadOfStaleNodes)
{
    auto document = CreateResourceGraph();
    std::string error;
    auto sources = document.Library.Build(error);
    document.OutputSource.clear();
    document.Machine.InitialState = "Idle";
    document.Machine.States = {{"Idle", sources.at("Clip0")}};
    document.StateSources = {"Clip0"};
    document.Positions = {{0, 0}};
    for (auto& key : document.Library.Sequences[0].Tracks[0].Translations)
        key.Value.x = 7;
    const auto graph = document.Compile();
    ASSERT_TRUE(graph);
    SLocalPose reference;
    reference.LocalTransforms.resize(1);
    const auto preview = graph->CreateAnimation(reference);
    ASSERT_TRUE(preview);
    ASSERT_TRUE(preview->Update(0.1f));
    EXPECT_NEAR(preview->GetLocalPose().LocalTransforms[0].Translation.x, 7, 0.001f);
}
