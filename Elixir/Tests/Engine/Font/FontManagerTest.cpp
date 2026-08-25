#include <gtest/gtest.h>

#include <Engine/Font/FontManager.h>

using namespace Elixir;

namespace
{
    Ref<Font> CreateTestFont()
    {
        return CreateRef<Font>(SFontCreateInfo{
            .Name = "Test",
            .Atlas = { .Info = { .PxRange = 1.0f, .Width = 1, .Height = 1 } },
            .Glyphs = {
                { .Unicode = ' ', .Advance = 0.5f },
                { .Unicode = 'a', .Advance = 1.0f },
                { .Unicode = 'b', .Advance = 1.0f },
                { .Unicode = 0x00E9, .Advance = 1.0f },
            },
            .AscenderY = 1.0f,
            .DescenderY = 0.0f,
        });
    }
}

TEST(FontManagerTest, MeasureWrappedBreaksAtWhitespace)
{
    const auto font = CreateTestFont();
    std::vector<std::string> lines;

    const glm::vec2 size = FontManager::MeasureWrapped("aa aa", font, 10.0f, 25.0f, &lines);

    EXPECT_EQ(lines, (std::vector<std::string>{ "aa", "aa" }));
    EXPECT_EQ(size, glm::vec2(20.0f, 20.0f));
}

TEST(FontManagerTest, MeasureWrappedBreaksOverlongWords)
{
    const auto font = CreateTestFont();
    std::vector<std::string> lines;

    const glm::vec2 size = FontManager::MeasureWrapped("aaa", font, 10.0f, 15.0f, &lines);

    EXPECT_EQ(lines, (std::vector<std::string>{ "a", "a", "a" }));
    EXPECT_EQ(size, glm::vec2(10.0f, 30.0f));
}

TEST(FontManagerTest, MeasureWrappedDoesNotSplitUtf8Characters)
{
    const auto font = CreateTestFont();
    const std::string accented = "\xC3\xA9";
    std::vector<std::string> lines;

    const glm::vec2 size = FontManager::MeasureWrapped(
        accented + accented,
        font,
        10.0f,
        15.0f,
        &lines
    );

    EXPECT_EQ(lines, (std::vector<std::string>{ accented, accented }));
    EXPECT_EQ(size, glm::vec2(10.0f, 20.0f));
}

TEST(FontManagerTest, MeasureWrappedPreservesExplicitAndEmptyLines)
{
    const auto font = CreateTestFont();
    std::vector<std::string> lines;

    const glm::vec2 size = FontManager::MeasureWrapped("a\n\nb", font, 10.0f, 100.0f, &lines);

    EXPECT_EQ(lines, (std::vector<std::string>{ "a", "", "b" }));
    EXPECT_EQ(size, glm::vec2(10.0f, 30.0f));
}

TEST(FontManagerTest, MeasureWrappedPreservesLeadingWhitespace)
{
    const auto font = CreateTestFont();
    std::vector<std::string> lines;

    const glm::vec2 size = FontManager::MeasureWrapped(" a\n\ta", font, 10.0f, 100.0f, &lines);

    EXPECT_EQ(lines, (std::vector<std::string>{ " a", "\ta" }));
    EXPECT_EQ(size, glm::vec2(15.0f, 20.0f));
}
