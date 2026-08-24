#include <gtest/gtest.h>
using namespace testing;

#include "ManagerTestUtils.h"

#include <Engine/GUI/Icon.h>
using namespace Elixir;
using namespace Elixir::GUI;

TEST(IconTest, IconStartsAsASelfHitTestInvisibleVisual)
{
    const GUI::Icon icon;

    EXPECT_EQ(icon.GetVisibility(), EVisibility::SelfHitTestInvisible);
}

TEST(IconStyleTest, RequestedStateMaterializesFromNormal)
{
    SIconStyle style;
    style.Get(EStyleLayer::Normal).Foreground = { 1.0f, 0.0f, 0.0f, 1.0f };
    style.Get(EStyleLayer::Hovered).Foreground = { 0.0f, 1.0f, 0.0f, 1.0f };

    ASSERT_TRUE(style.Hovered);
    EXPECT_EQ(style.Normal.Foreground, SColor(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_EQ(style.Hovered->Foreground, SColor(0.0f, 1.0f, 0.0f, 1.0f));
}

TEST(IconTest, ColorSetterMarksTheIconForRerender)
{
    const auto icon = CreateRef<GUI::Icon>();
    TestGUIManager manager;
    manager.SetRoot(icon);
    manager.AssembleFrame();
    ASSERT_FALSE(icon->IsRenderDirty());

    icon->SetColor(EStyleLayer::Hovered, { 0.0f, 1.0f, 0.0f, 1.0f });

    EXPECT_TRUE(icon->IsRenderDirty());
}
