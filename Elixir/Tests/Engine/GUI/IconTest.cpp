#include <gtest/gtest.h>
using namespace testing;

#include <Engine/GUI/Icon.h>
#include <Engine/GUI/IconLibrary.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    class TestIconLoader final : public IconLoader
    {
      public:
        EIconFormat GetFormat() const override { return EIconFormat::PNG; }

        Ref<IconContent> Load(const SIconSource&) const override { return nullptr; }
    };
}

TEST(IconTest, IconStartsAsASelfHitTestInvisibleVisual)
{
    const Icon icon;

    EXPECT_EQ(icon.GetVisibility(), EVisibility::SelfHitTestInvisible);
}

TEST(IconTest, ColorSetterMaterializesTheRequestedStateFromNormal)
{
    Icon icon;
    icon.SetColor(EStyleLayer::Normal, { 1.0f, 0.0f, 0.0f, 1.0f });
    icon.SetColor(EStyleLayer::Hovered, { 0.0f, 1.0f, 0.0f, 1.0f });

    ASSERT_TRUE(icon.GetStyle().Hovered);
    EXPECT_EQ(icon.GetStyle().Normal.Foreground, SColor(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_EQ(icon.GetStyle().Hovered->Foreground, SColor(0.0f, 1.0f, 0.0f, 1.0f));
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
    IconLibrary::Shutdown();

    EXPECT_TRUE(IconLibrary::RegisterLoader(CreateScope<TestIconLoader>()));
    EXPECT_FALSE(IconLibrary::RegisterLoader(CreateScope<TestIconLoader>()));
    EXPECT_TRUE(IconLibrary::ReplaceLoader(CreateScope<TestIconLoader>()));

    IconLibrary::Shutdown();
}
