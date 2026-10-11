#include <VFX/VFXDocument.h>
#include <VFX/VFXTimeline.h>
#include <gtest/gtest.h>
#include <chrono>
#include <fstream>

using namespace EditorVFX;

namespace
{
    class VFXDocumentTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            Directory = std::filesystem::temp_directory_path() / ("elixir-vfx-document-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(Directory);
            File = Directory / "Effect.json";
            ASSERT_TRUE(DocumentValue.Create(File,
                R"({"name":"Test","emitters":[],"metadata":{"note":"Keep me","id":18446744073709551615},"parameters":{"rate":10}})", Error));
            ASSERT_TRUE(DocumentValue.Append({"emitters"}, MakeEmitter("Emitter")));
        }
        void TearDown() override
        {
            std::error_code error; std::filesystem::remove_all(Directory, error);
        }
        std::filesystem::path Directory, File;
        Document DocumentValue;
        std::string Error;
    };

    TEST_F(VFXDocumentTest, SavesUnknownFieldsAndExactIntegersAcrossReload)
    {
        ASSERT_TRUE(DocumentValue.Save(Error)) << Error;
        EXPECT_FALSE(DocumentValue.Dirty());
        Document loaded;
        ASSERT_TRUE(loaded.Load(File, Error)) << Error;
        EXPECT_EQ(loaded.Get({"metadata", "id"})->Text, "18446744073709551615");
        EXPECT_EQ(loaded.Get({"metadata", "note"})->Text, "Keep me");
        EXPECT_EQ(loaded.Root().JSON(), DocumentValue.Root().JSON());
    }

    TEST_F(VFXDocumentTest, AppliesFieldsAtomicallyAndRejectsMalformedValues)
    {
        const auto before = DocumentValue.Root().JSON();
        EXPECT_FALSE(DocumentValue.Apply({{{"parameters", "rate"}, "20"}, {{"emitters", "0", "maxParticles"}, "broken"}}, Error));
        EXPECT_EQ(DocumentValue.Root().JSON(), before);
        ASSERT_TRUE(DocumentValue.Apply({{{"parameters", "rate"}, "20"}, {{"emitters", "0", "spawnRate"}, "$rate"}}, Error));
        EXPECT_EQ(DocumentValue.Get({"emitters", "0", "spawnRate"})->Text, "$rate");
        ASSERT_TRUE(DocumentValue.Undo()); EXPECT_EQ(DocumentValue.Root().JSON(), before);
        ASSERT_TRUE(DocumentValue.Redo()); EXPECT_EQ(DocumentValue.Get({"parameters", "rate"})->Text, "20");
        ASSERT_TRUE(DocumentValue.Apply({{{"emitters", "0", "spawnRate"}, "12.5"}}, Error));
        EXPECT_EQ(DocumentValue.Get({"emitters", "0", "spawnRate"})->Type, EValueType::Number);
        EXPECT_EQ(DocumentValue.Get({"emitters", "0", "spawnRate"})->Text, "12.5");
    }

    TEST_F(VFXDocumentTest, PreservesModuleOrderAcrossMovesDeletesAndHistory)
    {
        const Path path{"emitters", "0", "spawnModules"};
        const auto original = DocumentValue.Get(path)->JSON();
        EXPECT_FALSE(DocumentValue.Move(path, 0, -1));
        ASSERT_TRUE(DocumentValue.Move(path, 0, 1));
        EXPECT_EQ(DocumentValue.Get(path)->Array[1].String("type"), "SetLifetime");
        ASSERT_TRUE(DocumentValue.Remove(path, 1));
        ASSERT_TRUE(DocumentValue.Undo()); ASSERT_TRUE(DocumentValue.Undo());
        EXPECT_EQ(DocumentValue.Get(path)->JSON(), original);
    }

    TEST_F(VFXDocumentTest, RefusesToOverwriteExternalEdits)
    {
        ASSERT_TRUE(DocumentValue.Save(Error));
        std::ofstream(File) << R"({"name":"External","emitters":[]})";
        EXPECT_FALSE(DocumentValue.Save(Error));
        Document external; ASSERT_TRUE(external.Load(File, Error));
        EXPECT_EQ(external.Root().String("name"), "External");
    }

    TEST_F(VFXDocumentTest, UndoTracksTheSavedVersionAndNewEditsClearRedo)
    {
        ASSERT_TRUE(DocumentValue.Save(Error));
        ASSERT_TRUE(DocumentValue.Apply({{{"parameters", "rate"}, "30"}}, Error)); EXPECT_TRUE(DocumentValue.Dirty());
        ASSERT_TRUE(DocumentValue.Undo()); EXPECT_FALSE(DocumentValue.Dirty());
        ASSERT_TRUE(DocumentValue.Apply({{{"parameters", "rate"}, "40"}}, Error)); EXPECT_FALSE(DocumentValue.CanRedo());
    }

    TEST_F(VFXDocumentTest, RejectsDuplicateEmitterNamesWithoutChangingTheDocument)
    {
        ASSERT_TRUE(DocumentValue.Append({"emitters"}, MakeEmitter("Second")));
        const auto before = DocumentValue.Root().JSON();
        EXPECT_FALSE(DocumentValue.Apply({{{"emitters", "1", "name"}, "Emitter"}}, Error));
        EXPECT_EQ(DocumentValue.Root().JSON(), before);
    }

    TEST(VFXValueTest, EscapesStringsAndRoundTripsVectors)
    {
        const auto value = Value::FromString("Quote \" and newline\n\\");
        Value parsed; std::string error;
        ASSERT_TRUE(Value::Parse(value.JSON(), parsed, error)); EXPECT_EQ(value.Text, parsed.Text);
        ASSERT_TRUE(Value::Parse("[1,2,3,4]", parsed, error)); EXPECT_EQ(parsed.Array.size(), 4u);
    }
}

TEST(VFXTimelineTest, ResolvesSourceBurstPeriodAndTriggerDelay)
{
    Value system; std::string error;
    ASSERT_TRUE(Value::Parse(R"({"parameters":{"life":0.25},"emitters":[
        {"name":"Rocket","spawnRate":0,"burst":{"count":1,"interval":1.25}},
        {"name":"Explosion","spawnRate":0,"burst":{"count":10,"interval":9},
         "trigger":{"source":"Rocket","delay":1.18},
         "spawnModules":[{"type":"SetLifetime","max":"$life"}]}]})", system, error));
    const auto intervals = EmitterTimeline(system, 1, 5);
    ASSERT_EQ(intervals.size(), 3);
    EXPECT_NEAR(intervals[0].Start, 2.43f, 0.0001f);
    EXPECT_NEAR(intervals[0].End, 2.68f, 0.0001f);
    EXPECT_NEAR(intervals[2].End, 5, 0.0001f);
}

TEST(VFXTimelineTest, ContinuousEmissionFillsPreviewAndDisabledSourcesDoNotTrigger)
{
    Value system; std::string error;
    ASSERT_TRUE(Value::Parse(R"({"emitters":[
        {"name":"Smoke","spawnRate":20},
        {"name":"Rocket","enabled":false,"burst":{"count":1,"interval":1}},
        {"name":"Explosion","burst":{"count":10},"trigger":{"source":"Rocket","delay":0.5}}]})", system, error));
    const auto continuous = EmitterTimeline(system, 0, 5);
    ASSERT_EQ(continuous.size(), 1);
    EXPECT_EQ(continuous[0].Start, 0);
    EXPECT_EQ(continuous[0].End, 5);
    EXPECT_TRUE(EmitterTimeline(system, 1, 5).empty());
    EXPECT_TRUE(EmitterTimeline(system, 2, 5).empty());
}

TEST(VFXTimelineTest, LocalLifetimeOverridesSystemParameterAndOverlappingBurstsMerge)
{
    Value system; std::string error;
    ASSERT_TRUE(Value::Parse(R"({"parameters":{"life":0.1},"emitters":[
        {"parameters":{"life":2},"burst":{"count":1,"interval":1},
         "spawnModules":[{"type":"SetLifetime","max":"$life"}]}]})", system, error));
    const auto intervals = EmitterTimeline(system, 0, 5);
    ASSERT_EQ(intervals.size(), 1);
    EXPECT_EQ(intervals[0].Start, 1);
    EXPECT_EQ(intervals[0].End, 5);
}
