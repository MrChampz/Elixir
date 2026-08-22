#include <gtest/gtest.h>
using namespace testing;

#include <Engine/GUI/Icon.h>
using namespace Elixir;
using namespace Elixir::GUI;

TEST(IconTest, IconStartsAsASelfHitTestInvisibleVisual)
{
    const GUI::Icon icon;

    EXPECT_EQ(icon.GetVisibility(), EVisibility::SelfHitTestInvisible);
}

TEST(IconTest, ColorSetterMaterializesTheRequestedStateFromNormal)
{
    GUI::Icon icon;
    icon.SetColor(EStyleLayer::Normal, { 1.0f, 0.0f, 0.0f, 1.0f });
    icon.SetColor(EStyleLayer::Hovered, { 0.0f, 1.0f, 0.0f, 1.0f });

    ASSERT_TRUE(icon.GetStyle().Hovered);
    EXPECT_EQ(icon.GetStyle().Normal.Foreground, SColor(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_EQ(icon.GetStyle().Hovered->Foreground, SColor(0.0f, 1.0f, 0.0f, 1.0f));
}
