#include <gtest/gtest.h>
using namespace testing;

#include "ManagerTestUtils.h"

#include <Engine/Event/MouseEvent.h>
#include <Engine/GUI/Style.h>
#include <Engine/GUI/VerticalBox.h>
#include <Engine/Graphics/Texture.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // A non-null Ref<Texture2D> that is never dereferenced - StyleSet::Resolve only ever
    // copies and compares the pointer, so a real GPU-backed texture (which would need a
    // GraphicsContext this test suite doesn't have) isn't needed to prove identity.
    Ref<Texture2D> FakeTexture()
    {
        return { reinterpret_cast<Texture2D*>(0x1), [](Texture2D*) {} };
    }

    // Minimal leaf that promotes the protected input handlers a real widget would normally
    // only receive through Manager routing, so this test can drive Hovered/Pressed/Enabled
    // directly without needing a full hit-test pass.
    class StyleLeaf final : public Widget
    {
      public:
        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return { 10.0f, 10.0f }; }

        using Widget::HandleMouseEnter;
        using Widget::HandleMouseLeave;
        using Widget::HandleMouseDown;
    };
}

// --- StyleSet::Resolve: pure composition logic, no window/render/input involved ---

TEST(StyleTest, NormalOnlyReturnsNormalValues)
{
    StyleSet styles;

    SStyleOverride normal;
    normal.BackgroundColor = SColor{ 1.0f, 0.0f, 0.0f, 1.0f };
    normal.CornerRadius = glm::vec4{ 4.0f };
    styles.Set(EStyleLayer::Normal, normal);

    const SResolvedStyle resolved = styles.Resolve(EInteractionState::None);

    EXPECT_EQ(resolved.BackgroundColor, normal.BackgroundColor);
    EXPECT_EQ(resolved.CornerRadius, *normal.CornerRadius);
}

TEST(StyleTest, HoveredOverridesOnlyTheFieldsItDeclares)
{
    StyleSet styles;

    SStyleOverride normal;
    normal.BackgroundColor = SColor{ 1.0f, 0.0f, 0.0f, 1.0f };
    normal.BackgroundBorders = glm::vec4{ 30.0f };
    normal.Outline = SOutline{ SColor{ 0.0f, 0.0f, 0.0f, 1.0f }, 1.0f };
    styles.Set(EStyleLayer::Normal, normal);

    SStyleOverride hovered;
    hovered.BackgroundColor = SColor{ 0.0f, 1.0f, 0.0f, 1.0f };
    styles.Set(EStyleLayer::Hovered, hovered);

    const SResolvedStyle resolved = styles.Resolve(EInteractionState::Hovered);

    EXPECT_EQ(resolved.BackgroundColor, hovered.BackgroundColor)
        << "Hovered declares BackgroundColor, so it must win";
    EXPECT_EQ(resolved.BackgroundBorders, *normal.BackgroundBorders)
        << "Hovered never declared BackgroundBorders, so Normal's value must still show";
    EXPECT_EQ(resolved.Outline.Thickness, normal.Outline->Thickness)
        << "Hovered never declared Outline, so Normal's value must still show";
}

TEST(StyleTest, PressedWinsOverHoveredWhenBothActive)
{
    StyleSet styles;

    SStyleOverride normal;
    normal.BackgroundColor = SColor{ 1.0f, 0.0f, 0.0f, 1.0f };
    styles.Set(EStyleLayer::Normal, normal);

    SStyleOverride hovered;
    hovered.BackgroundColor = SColor{ 0.0f, 1.0f, 0.0f, 1.0f };
    styles.Set(EStyleLayer::Hovered, hovered);

    SStyleOverride pressed;
    pressed.BackgroundColor = SColor{ 0.0f, 0.0f, 1.0f, 1.0f };
    styles.Set(EStyleLayer::Pressed, pressed);

    const SResolvedStyle resolved = styles.Resolve(
        EInteractionState::Hovered | EInteractionState::Pressed
    );

    EXPECT_EQ(resolved.BackgroundColor, pressed.BackgroundColor);
}

TEST(StyleTest, FocusedWinsOverPressedAndHovered)
{
    StyleSet styles;

    SStyleOverride normal;
    normal.BackgroundColor = SColor{ 1.0f, 0.0f, 0.0f, 1.0f };
    styles.Set(EStyleLayer::Normal, normal);

    SStyleOverride pressed;
    pressed.BackgroundColor = SColor{ 0.0f, 0.0f, 1.0f, 1.0f };
    styles.Set(EStyleLayer::Pressed, pressed);

    SStyleOverride focused;
    focused.BackgroundColor = SColor{ 1.0f, 1.0f, 0.0f, 1.0f };
    styles.Set(EStyleLayer::Focused, focused);

    const SResolvedStyle resolved = styles.Resolve(
        EInteractionState::Hovered | EInteractionState::Pressed | EInteractionState::Focused
    );

    EXPECT_EQ(resolved.BackgroundColor, focused.BackgroundColor);
}

TEST(StyleTest, DisabledStillWinsOverFocused)
{
    StyleSet styles;

    SStyleOverride normal;
    normal.BackgroundColor = SColor{ 1.0f, 0.0f, 0.0f, 1.0f };
    styles.Set(EStyleLayer::Normal, normal);

    SStyleOverride focused;
    focused.BackgroundColor = SColor{ 1.0f, 1.0f, 0.0f, 1.0f };
    styles.Set(EStyleLayer::Focused, focused);

    SStyleOverride disabled;
    disabled.BackgroundColor = SColor{ 0.5f, 0.5f, 0.5f, 1.0f };
    styles.Set(EStyleLayer::Disabled, disabled);

    // A widget can stay focused after being disabled - nothing clears focus just because
    // IsEnabled() went false - so Disabled has to keep winning even then.
    const SResolvedStyle resolved = styles.Resolve(
        EInteractionState::Focused | EInteractionState::Disabled
    );

    EXPECT_EQ(resolved.BackgroundColor, disabled.BackgroundColor);
}

TEST(StyleTest, DisabledWinsOverPressedAndHovered)
{
    StyleSet styles;

    SStyleOverride normal;
    normal.BackgroundColor = SColor{ 1.0f, 0.0f, 0.0f, 1.0f };
    styles.Set(EStyleLayer::Normal, normal);

    SStyleOverride hovered;
    hovered.BackgroundColor = SColor{ 0.0f, 1.0f, 0.0f, 1.0f };
    styles.Set(EStyleLayer::Hovered, hovered);

    SStyleOverride pressed;
    pressed.BackgroundColor = SColor{ 0.0f, 0.0f, 1.0f, 1.0f };
    styles.Set(EStyleLayer::Pressed, pressed);

    SStyleOverride disabled;
    disabled.BackgroundColor = SColor{ 0.5f, 0.5f, 0.5f, 1.0f };
    styles.Set(EStyleLayer::Disabled, disabled);

    const SResolvedStyle resolved = styles.Resolve(
        EInteractionState::Hovered | EInteractionState::Pressed | EInteractionState::Disabled
    );

    EXPECT_EQ(resolved.BackgroundColor, disabled.BackgroundColor);
}

TEST(StyleTest, DisabledFallsBackToTheLastLayerThatDeclaresAField)
{
    StyleSet styles;

    SStyleOverride normal;
    normal.BackgroundColor = SColor{ 1.0f, 0.0f, 0.0f, 1.0f };
    styles.Set(EStyleLayer::Normal, normal);

    SStyleOverride pressed;
    pressed.BackgroundColor = SColor{ 0.0f, 0.0f, 1.0f, 1.0f };
    styles.Set(EStyleLayer::Pressed, pressed);

    // Disabled is active but declares nothing for this field.
    styles.Set(EStyleLayer::Disabled, SStyleOverride{});

    const SResolvedStyle resolved = styles.Resolve(
        EInteractionState::Pressed | EInteractionState::Disabled
    );

    EXPECT_EQ(resolved.BackgroundColor, pressed.BackgroundColor)
        << "Disabled declared nothing, so the last layer that did (Pressed) must still show";
}

TEST(StyleTest, EmptyTextureOverrideExplicitlyClearsAnInheritedTexture)
{
    StyleSet styles;

    SStyleOverride normal;
    normal.BackgroundTexture = FakeTexture();
    styles.Set(EStyleLayer::Normal, normal);

    SStyleOverride pressed;
    pressed.BackgroundTexture = Ref<Texture2D>{}; // present, but null: an explicit clear
    styles.Set(EStyleLayer::Pressed, pressed);

    const SResolvedStyle resolved = styles.Resolve(EInteractionState::Pressed);

    EXPECT_EQ(resolved.BackgroundTexture, nullptr);
}

TEST(StyleTest, InactiveLayerNeverLeaksIntoTheResolvedStyle)
{
    StyleSet styles;

    SStyleOverride normal;
    normal.BackgroundColor = SColor{ 1.0f, 0.0f, 0.0f, 1.0f };
    styles.Set(EStyleLayer::Normal, normal);

    SStyleOverride pressed;
    pressed.BackgroundColor = SColor{ 0.0f, 0.0f, 1.0f, 1.0f };
    styles.Set(EStyleLayer::Pressed, pressed);

    // Hovered active, Pressed not - Pressed's color must not appear.
    const SResolvedStyle resolved = styles.Resolve(EInteractionState::Hovered);

    EXPECT_EQ(resolved.BackgroundColor, normal.BackgroundColor);
}

// --- Widget's public style/enabled surface: dirty-marking and interaction gating ---

TEST(StyleTest, SettingAStyleMarksTheWidgetForRerender)
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

TEST(StyleTest, HoverPressAndEnabledChangesEachMarkTheWidgetForRerender)
{
    const auto root = CreateRef<VerticalBox>();
    const auto leaf = CreateRef<StyleLeaf>();
    root->AddChild(leaf);

    TestGUIManager manager;
    manager.SetRoot(root);

    manager.AssembleFrame();
    leaf->HandleMouseEnter();
    EXPECT_TRUE(leaf->IsRenderDirty()) << "entering hover must mark for rerender";

    manager.AssembleFrame();
    leaf->HandleMouseLeave();
    EXPECT_TRUE(leaf->IsRenderDirty()) << "leaving hover must mark for rerender";

    manager.AssembleFrame();
    leaf->SetEnabled(false);
    EXPECT_TRUE(leaf->IsRenderDirty()) << "disabling must mark for rerender";
}

TEST(StyleTest, DisablingBlocksInteractionThatWouldOtherwiseBeHandled)
{
    const auto leaf = CreateRef<StyleLeaf>();
    leaf->OnClick([] {}); // gives HandleMouseDown a reason to accept the press at all

    const MouseButtonPressedEvent event(0);

    ASSERT_TRUE(leaf->HandleMouseDown(event).EventHandled)
        << "sanity check: an enabled widget with a click handler must accept the press";

    leaf->SetEnabled(false);

    EXPECT_FALSE(leaf->HandleMouseDown(event).EventHandled)
        << "a disabled widget must refuse the press even though it would normally handle it";
}
