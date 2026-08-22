#include <gtest/gtest.h>
using namespace testing;

#include "ManagerTestUtils.h"

#include <Engine/Event/MouseEvent.h>
#include <Engine/GUI/Button.h>
#include <Engine/GUI/Checkbox.h>
#include <Engine/GUI/Style.h>
#include <Engine/GUI/VerticalBox.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    class StyleLeaf final : public Widget
    {
      public:
        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return { 10.0f, 10.0f }; }

        using Widget::HandleMouseDown;
        using Widget::HandleMouseEnter;
        using Widget::HandleMouseLeave;
    };
}

TEST(StyleTest, StateStylesResolveByInteractionPriority)
{
    TStateStyles<SButtonAppearance> styles;
    styles.Normal.Foreground = { 1.0f, 0.0f, 0.0f, 1.0f };
    styles.Hovered.Foreground = { 0.0f, 1.0f, 0.0f, 1.0f };
    styles.Pressed.Foreground = { 0.0f, 0.0f, 1.0f, 1.0f };
    styles.Disabled.Foreground = { 0.5f, 0.5f, 0.5f, 1.0f };

    EXPECT_EQ(styles.Resolve(EInteractionState::None).Foreground, styles.Normal.Foreground);
    EXPECT_EQ(styles.Resolve(EInteractionState::Hovered).Foreground, styles.Hovered.Foreground);
    EXPECT_EQ(
        styles.Resolve(EInteractionState::Hovered | EInteractionState::Pressed).Foreground,
        styles.Pressed.Foreground
    );
    EXPECT_EQ(
        styles.Resolve(EInteractionState::Hovered | EInteractionState::Pressed | EInteractionState::Disabled).Foreground,
        styles.Disabled.Foreground
    );
}

TEST(StyleTest, CheckboxOwnsCheckedStateResolution)
{
    SCheckboxStyle styles;
    styles.Normal.Background.Color = { 1.0f, 0.0f, 0.0f, 1.0f };
    styles.Checked.Background.Color = { 0.0f, 1.0f, 0.0f, 1.0f };
    styles.CheckedHovered.Background.Color = { 0.0f, 0.0f, 1.0f, 1.0f };
    styles.CheckedDisabled.Background.Color = { 0.5f, 0.5f, 0.5f, 1.0f };

    EXPECT_EQ(styles.Resolve(false, EInteractionState::None).Background.Color, styles.Normal.Background.Color);
    EXPECT_EQ(styles.Resolve(true, EInteractionState::None).Background.Color, styles.Checked.Background.Color);
    EXPECT_EQ(styles.Resolve(true, EInteractionState::Hovered).Background.Color, styles.CheckedHovered.Background.Color);
    EXPECT_EQ(
        styles.Resolve(true, EInteractionState::Hovered | EInteractionState::Disabled).Background.Color,
        styles.CheckedDisabled.Background.Color
    );
}

TEST(StyleTest, StyleSetStoresStylesByConcreteType)
{
    StyleSet styles;
    SButtonStyle button;
    button.Normal.Foreground = { 0.1f, 0.2f, 0.3f, 1.0f };

    styles.SetWidgetStyle(button);

    EXPECT_EQ(styles.GetWidgetStyle<SButtonStyle>().Normal.Foreground, button.Normal.Foreground);
}

TEST(StyleTest, WidgetOwnsTheStyleItReceives)
{
    StyleLeaf leaf;
    SWidgetStyle style;
    style.Normal.Background.Color = { 1.0f, 0.0f, 0.0f, 1.0f };

    leaf.SetStyle(style);
    leaf.SetBackgroundColor(EStyleLayer::Hovered, { 0.0f, 1.0f, 0.0f, 1.0f });

    EXPECT_EQ(leaf.GetStyle().Normal.Background.Color, style.Normal.Background.Color);
    EXPECT_EQ(leaf.GetStyle().Hovered.Background.Color, SColor(0.0f, 1.0f, 0.0f, 1.0f));
}

TEST(StyleTest, LegacyBackgroundSetterMarksTheWidgetForRerender)
{
    const auto root = CreateRef<VerticalBox>();
    const auto leaf = CreateRef<StyleLeaf>();
    root->AddChild(leaf);

    TestGUIManager manager;
    manager.SetRoot(root);
    manager.AssembleFrame();
    ASSERT_FALSE(leaf->IsRenderDirty());

    leaf->SetBackgroundColor(EStyleLayer::Normal, { 1.0f, 0.0f, 0.0f, 1.0f });

    EXPECT_TRUE(leaf->IsRenderDirty());
}

TEST(StyleTest, HoverPressAndEnabledChangesMarkTheWidgetForRerender)
{
    const auto root = CreateRef<VerticalBox>();
    const auto leaf = CreateRef<StyleLeaf>();
    root->AddChild(leaf);

    TestGUIManager manager;
    manager.SetRoot(root);

    manager.AssembleFrame();
    leaf->HandleMouseEnter();
    EXPECT_TRUE(leaf->IsRenderDirty());

    manager.AssembleFrame();
    leaf->HandleMouseLeave();
    EXPECT_TRUE(leaf->IsRenderDirty());

    manager.AssembleFrame();
    leaf->SetEnabled(false);
    EXPECT_TRUE(leaf->IsRenderDirty());
}

TEST(StyleTest, DisablingBlocksInteractionThatWouldOtherwiseBeHandled)
{
    const auto leaf = CreateRef<StyleLeaf>();
    leaf->OnClick([] {});

    const MouseButtonPressedEvent event(0);
    ASSERT_TRUE(leaf->HandleMouseDown(event).EventHandled);

    leaf->SetEnabled(false);

    EXPECT_FALSE(leaf->HandleMouseDown(event).EventHandled);
}
