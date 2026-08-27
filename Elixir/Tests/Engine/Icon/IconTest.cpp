#include <gtest/gtest.h>
using namespace testing;

#include <Engine/Icon/IconManager.h>
using namespace Elixir;

namespace
{
    class TestIconLoader final : public IconLoader
    {
      public:
        EIconFormat GetFormat() const override { return EIconFormat::PNG; }

        Ref<IconContent> Load(const SIconSource&) const override { return nullptr; }
    };
}

TEST(IconTest, BitmapValidityRequiresExactlyOneRgbaBuffer)
{
    SIconBitmap bitmap;
    bitmap.Size = { 2, 3 };
    bitmap.Pixels.resize(23);
    EXPECT_FALSE(bitmap.IsValid());

    bitmap.Pixels.resize(24);
    EXPECT_TRUE(bitmap.IsValid());
}

TEST(IconTest, RegisterLoaderRejectsDuplicateFormatAndReplacementIsExplicit)
{
    IconManager::Shutdown();

    EXPECT_TRUE(IconManager::RegisterLoader(CreateScope<TestIconLoader>()));
    EXPECT_FALSE(IconManager::RegisterLoader(CreateScope<TestIconLoader>()));
    EXPECT_TRUE(IconManager::ReplaceLoader(CreateScope<TestIconLoader>()));

    IconManager::Shutdown();
}
