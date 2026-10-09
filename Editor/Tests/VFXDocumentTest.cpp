#include <VFX/VFXDocument.h>
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
