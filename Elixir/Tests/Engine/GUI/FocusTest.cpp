#include <gtest/gtest.h>
using namespace testing;

#include "ManagerTestUtils.h"

#include <Engine/GUI/VerticalBox.h>
#include <Engine/Input/InputCodes.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // Minimal leaf used to populate a focus order - SetFocusable is public on Widget, so
    // no subclassing is needed just to opt a widget into Tab navigation (unlike
    // ScrollBoxTest.cpp's TestScrollBox, which promotes protected overrides).
    class FocusLeaf final : public Widget
    {
      public:
        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return { 10.0f, 10.0f }; }
    };

    KeyPressedEvent TabEvent(const bool shift = false)
    {
        return KeyPressedEvent(EE_KEY_TAB, 0, false, false, shift);
    }

    KeyPressedEvent EscapeEvent()
    {
        return KeyPressedEvent(EE_KEY_ESCAPE, 0, false, false, false);
    }
}

TEST(FocusTest, TabVisitsOnlyFocusableVisibleWidgetsInChildOrder)
{
    const auto root = CreateRef<VerticalBox>();

    const auto a = CreateRef<FocusLeaf>();
    a->SetFocusable(true);

    const auto notFocusable = CreateRef<FocusLeaf>();
    // notFocusable never calls SetFocusable - default is false (Widget.h).

    const auto hidden = CreateRef<FocusLeaf>();
    hidden->SetFocusable(true);
    hidden->SetVisibility(EVisibility::Hidden);

    const auto collapsed = CreateRef<FocusLeaf>();
    collapsed->SetFocusable(true);
    collapsed->SetVisibility(EVisibility::Collapsed);

    const auto b = CreateRef<FocusLeaf>();
    b->SetFocusable(true);

    root->AddChild(a);
    root->AddChild(notFocusable);
    root->AddChild(hidden);
    root->AddChild(collapsed);
    root->AddChild(b);

    TestGUIManager manager;
    manager.SetRoot(root);

    // Nothing focused yet: Tab must land on the first focusable widget in child order (a),
    // not b or either of the skipped ones.
    manager.HandleKeyPressed(TabEvent());
    EXPECT_TRUE(a->IsFocused());
    EXPECT_FALSE(b->IsFocused());

    // From a, the next reachable widget is b - notFocusable/hidden/collapsed are all skipped.
    manager.HandleKeyPressed(TabEvent());
    EXPECT_FALSE(a->IsFocused());
    EXPECT_TRUE(b->IsFocused());
    EXPECT_FALSE(notFocusable->IsFocused());
    EXPECT_FALSE(hidden->IsFocused());
    EXPECT_FALSE(collapsed->IsFocused());
}

TEST(FocusTest, TabWrapsAroundFromLastToFirst)
{
    const auto root = CreateRef<VerticalBox>();

    const auto a = CreateRef<FocusLeaf>();
    a->SetFocusable(true);
    const auto b = CreateRef<FocusLeaf>();
    b->SetFocusable(true);

    root->AddChild(a);
    root->AddChild(b);

    TestGUIManager manager;
    manager.SetRoot(root);

    manager.SetFocusedWidget(b); // start at the last focusable widget

    manager.HandleKeyPressed(TabEvent());
    EXPECT_TRUE(a->IsFocused()) << "Tab from the last focusable widget must wrap to the first";
    EXPECT_FALSE(b->IsFocused());
}

TEST(FocusTest, ShiftTabWrapsAroundFromFirstToLast)
{
    const auto root = CreateRef<VerticalBox>();

    const auto a = CreateRef<FocusLeaf>();
    a->SetFocusable(true);
    const auto b = CreateRef<FocusLeaf>();
    b->SetFocusable(true);

    root->AddChild(a);
    root->AddChild(b);

    TestGUIManager manager;
    manager.SetRoot(root);

    manager.SetFocusedWidget(a); // start at the first focusable widget

    manager.HandleKeyPressed(TabEvent(/*shift=*/true));
    EXPECT_TRUE(b->IsFocused()) << "Shift+Tab from the first focusable widget must wrap to the last";
    EXPECT_FALSE(a->IsFocused());
}

TEST(FocusTest, EscapeClearsFocus)
{
    const auto root = CreateRef<VerticalBox>();
    const auto a = CreateRef<FocusLeaf>();
    a->SetFocusable(true);
    root->AddChild(a);

    TestGUIManager manager;
    manager.SetRoot(root);
    manager.SetFocusedWidget(a);
    ASSERT_TRUE(a->IsFocused());

    manager.HandleKeyPressed(EscapeEvent());
    EXPECT_FALSE(a->IsFocused());
}

// The central guarantee behind scoping BuildFocusOrder to the topmost layer: once a popup is
// open, Tab must never reach a widget that sits underneath it, even though that widget is
// still focusable and still in the tree - only PopPopup (or ClearPopups) can bring it back
// into reach. Same idiom PopupLayerTest.cpp uses to prove popups draw above the root: build
// content in two layers and check that behavior stays confined to the active one.
TEST(FocusTest, TabStaysScopedInsideOpenPopup)
{
    const auto root = CreateRef<VerticalBox>();
    const auto rootWidget = CreateRef<FocusLeaf>();
    rootWidget->SetFocusable(true);
    root->AddChild(rootWidget);

    const auto popupRoot = CreateRef<VerticalBox>();
    const auto popupWidget = CreateRef<FocusLeaf>();
    popupWidget->SetFocusable(true);
    popupRoot->AddChild(popupWidget);

    TestGUIManager manager;
    manager.SetRoot(root);
    manager.PushPopup(popupRoot, { { 0, 0 }, { 10, 10 } });

    // Nothing focused yet, topmost layer is the popup: Tab must land inside it, never on the
    // root layer's widget underneath.
    manager.HandleKeyPressed(TabEvent());
    EXPECT_TRUE(popupWidget->IsFocused());
    EXPECT_FALSE(rootWidget->IsFocused());

    // With only one focusable widget in the popup, Tab keeps cycling back to it - it must
    // never "spill over" into the root layer's widget.
    manager.HandleKeyPressed(TabEvent());
    EXPECT_TRUE(popupWidget->IsFocused());
    EXPECT_FALSE(rootWidget->IsFocused());
}

// Regression guard: SetFocusedWidget(nullptr) on a hit path with nothing in it - the
// existing "clicked outside" behavior ProcessMousePress already had before this point
// (Manager.cpp) - must keep working now that focus is also driven from HandleKeyPressed.
TEST(FocusTest, ClickOutsideStillClearsFocus)
{
    const auto root = CreateRef<VerticalBox>();
    const auto a = CreateRef<FocusLeaf>();
    a->SetFocusable(true);
    root->AddChild(a);

    TestGUIManager manager;
    manager.SetRoot(root);
    manager.SetFocusedWidget(a);
    ASSERT_TRUE(a->IsFocused());

    manager.ProcessMousePress({}); // empty hit path: nobody under the cursor
    EXPECT_FALSE(a->IsFocused());
}

TEST(FocusTest, TabWithNoFocusableWidgetsIsANoOp)
{
    const auto root = CreateRef<VerticalBox>();
    root->AddChild(CreateRef<FocusLeaf>()); // never made focusable

    TestGUIManager manager;
    manager.SetRoot(root);

    EXPECT_NO_FATAL_FAILURE(manager.HandleKeyPressed(TabEvent()));
}

// FocusNext/FocusPrevious deliberately don't fall back to SetFocusedWidget(nullptr) when the
// scoped order is empty (see the comment on Manager::FocusNext) - otherwise opening a popup
// with no focusable content of its own, then pressing Tab, would silently steal focus away
// from whatever was focused in the layer underneath, for no reason the user asked for.
TEST(FocusTest, TabInPopupWithNoFocusableContentLeavesOuterFocusUntouched)
{
    const auto root = CreateRef<VerticalBox>();
    const auto rootWidget = CreateRef<FocusLeaf>();
    rootWidget->SetFocusable(true);
    root->AddChild(rootWidget);

    // A popup whose only content is not focusable - the interesting case is not "no popup",
    // it's "popup exists and is the topmost layer, but contributes nothing to the order".
    const auto popupRoot = CreateRef<VerticalBox>();
    popupRoot->AddChild(CreateRef<FocusLeaf>()); // never made focusable

    TestGUIManager manager;
    manager.SetRoot(root);
    manager.SetFocusedWidget(rootWidget);
    manager.PushPopup(popupRoot, { { 0, 0 }, { 10, 10 } });
    ASSERT_TRUE(rootWidget->IsFocused());

    manager.HandleKeyPressed(TabEvent());
    EXPECT_TRUE(rootWidget->IsFocused())
        << "Tab in a popup with nothing focusable must not clear focus in the layer below it";
}
