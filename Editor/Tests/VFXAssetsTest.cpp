#include <Assets/VFXAssets.h>
#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

namespace
{
    class VFXAssetsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            Root = std::filesystem::temp_directory_path() / ("elixir-vfx-assets-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(Root);
        }

        void TearDown() override
        {
            std::error_code error;
            std::filesystem::remove_all(Root, error);
        }

        void CreateFile(const std::filesystem::path& path)
        {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream file(path);
            file << "{}";
        }

        std::filesystem::path Root;
    };

    TEST_F(VFXAssetsTest, FindsNestedEffectsUnderTheSpecifiedProjectRoot)
    {
        const auto directory = Root / "Assets" / "VFX";
        CreateFile(directory / "Z.json");
        CreateFile(directory / "A.json");
        CreateFile(directory / "Nested" / "Rain.JSON");
        CreateFile(directory / "Readme.txt");
        CreateFile(Root / "Assets" / "Materials" / "Other.json");
        std::filesystem::create_directory(directory / "Folder.json");

        const auto found = EditorAssets::FindVFXAssets(Root);
        EXPECT_TRUE(found.Error.empty());
        EXPECT_EQ(found.Files, (std::vector<std::filesystem::path>{
            directory / "A.json", directory / "Nested" / "Rain.JSON", directory / "Z.json"
        }));
    }

    TEST_F(VFXAssetsTest, EmptyVFXDirectoryIsNotAnError)
    {
        std::filesystem::create_directories(Root / "Assets" / "VFX");
        const auto found = EditorAssets::FindVFXAssets(Root);
        EXPECT_TRUE(found.Files.empty());
        EXPECT_TRUE(found.Error.empty());
    }

    TEST_F(VFXAssetsTest, ReportsMissingVFXDirectory)
    {
        const auto found = EditorAssets::FindVFXAssets(Root);
        EXPECT_TRUE(found.Files.empty());
        EXPECT_FALSE(found.Error.empty());
    }

    TEST_F(VFXAssetsTest, ReportsAFileWhereTheVFXDirectoryShouldBe)
    {
        CreateFile(Root / "Assets" / "VFX");
        const auto found = EditorAssets::FindVFXAssets(Root);
        EXPECT_TRUE(found.Files.empty());
        EXPECT_FALSE(found.Error.empty());
    }
}
