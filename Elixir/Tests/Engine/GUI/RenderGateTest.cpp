#include <gtest/gtest.h>
using namespace testing;

#include "ManagerTestUtils.h"

#include <Engine/GUI/VerticalBox.h>
using namespace Elixir;
using namespace Elixir::GUI;

// The Manager's frame gate skips re-assembly/upload while the dirty epoch is unchanged.
// These tests pin the epoch's contract: it advances on any change and stays put otherwise.

TEST(RenderGateTest, VisualSetterAdvancesDirtyEpoch)
{
    const auto box = CreateRef<VerticalBox>();

    const uint64_t before = Widget::CurrentDirtyEpoch();
    box->SetOpacity(0.5f); // purely visual
    EXPECT_GT(Widget::CurrentDirtyEpoch(), before);
}

TEST(RenderGateTest, LayoutSetterAdvancesDirtyEpoch)
{
    const auto box = CreateRef<VerticalBox>();

    const uint64_t before = Widget::CurrentDirtyEpoch();
    box->SetPadding(SPadding(5.0f)); // affects layout
    EXPECT_GT(Widget::CurrentDirtyEpoch(), before);
}

TEST(RenderGateTest, AssemblingAnUnchangedTreeDoesNotAdvanceEpoch)
{
    const auto box = CreateRef<VerticalBox>();
    box->ArrangeChildren({ { 0, 0 }, { 100, 100 } });

    TestGUIManager manager;
    manager.SetRoot(box);
    manager.AssembleFrame(); // consumes the initial render-dirty

    // With nothing changed, assembling again must not advance the epoch — otherwise the
    // Manager's gate would rebuild every frame and the optimisation would be defeated.
    const uint64_t after = Widget::CurrentDirtyEpoch();
    manager.AssembleFrame();
    EXPECT_EQ(Widget::CurrentDirtyEpoch(), after);
}

// The gate predicate itself. NeedsRebuild() must fire on the first frame, after any
// invalidation, and — crucially — when the root widget is swapped even if the global
// dirty epoch is unchanged (SetRoot does not invalidate anything).

TEST(RenderGateTest, FirstFrameNeedsRebuild)
{
    const auto box = CreateRef<VerticalBox>();

    TestGUIManager manager;
    manager.SetRoot(box);

    EXPECT_TRUE(manager.NeedsRebuild()) << "nothing has been rendered yet";
}

TEST(RenderGateTest, CleanFrameNeedsNoRebuild)
{
    const auto box = CreateRef<VerticalBox>();

    TestGUIManager manager;
    manager.SetRoot(box);
    manager.AssembleFrame();
    manager.MarkRebuilt();

    EXPECT_FALSE(manager.NeedsRebuild()) << "nothing changed since the last rebuild";
}

TEST(RenderGateTest, InvalidationAfterRebuildNeedsRebuild)
{
    const auto box = CreateRef<VerticalBox>();

    TestGUIManager manager;
    manager.SetRoot(box);
    manager.AssembleFrame();
    manager.MarkRebuilt();
    ASSERT_FALSE(manager.NeedsRebuild());

    box->SetBackgroundColor(EStyleLayer::Normal, SColor(1.0f, 0.0f, 0.0f, 1.0f)); // bumps the dirty epoch
    EXPECT_TRUE(manager.NeedsRebuild());
}

TEST(RenderGateTest, RootSwapTriggersRebuildEvenWhenEpochUnchanged)
{
    const auto a = CreateRef<VerticalBox>();
    const auto b = CreateRef<VerticalBox>();

    TestGUIManager manager;
    manager.SetRoot(a);
    manager.AssembleFrame();
    manager.MarkRebuilt();
    ASSERT_FALSE(manager.NeedsRebuild());

    // Swapping to another already-built tree invalidates nothing, so the global dirty
    // epoch stays put...
    const uint64_t epoch = Widget::CurrentDirtyEpoch();
    manager.SetRoot(b);
    ASSERT_EQ(Widget::CurrentDirtyEpoch(), epoch) << "SetRoot must not advance the epoch";

    // ...yet the gate must still rebuild, driven purely by the root-pointer change.
    EXPECT_TRUE(manager.NeedsRebuild());
}
TEST(RenderGateTest, CapturedCommandsRemainUnchangedWhenTheWidgetTreeAdvances)
{
    Manager manager;
    const auto root = CreateRef<VerticalBox>();
    const SColor firstColor{1, 0, 0, 1};
    const SColor nextColor{0, 0, 1, 1};
    root->SetBackgroundColor(EStyleLayer::Normal, firstColor);
    manager.SetRoot(root);
    manager.ArrangeLayout({100, 100});
    const auto first = manager.CaptureFrame();
    ASSERT_TRUE(first);
    ASSERT_FALSE(first->GetCommands().empty());
    EXPECT_EQ(first->GetCommands().front().Color, firstColor);
    root->SetBackgroundColor(EStyleLayer::Normal, nextColor);
    const auto next = manager.CaptureFrame();
    ASSERT_TRUE(next);
    ASSERT_FALSE(next->GetCommands().empty());
    EXPECT_NE(first, next);
    EXPECT_EQ(first->GetCommands().front().Color, firstColor);
    EXPECT_EQ(next->GetCommands().front().Color, nextColor);
}

TEST(RenderGateTest, UnchangedWidgetTreesReuseThePublishedSnapshot)
{
    Manager manager;
    manager.SetRoot(CreateRef<VerticalBox>());
    manager.ArrangeLayout({100, 100});
    const auto first = manager.CaptureFrame();
    ASSERT_TRUE(first);
    EXPECT_EQ(manager.CaptureFrame(), first);
}

TEST(RenderGateTest, HiddenRootsDoNotPublishDrawCommands)
{
    Manager manager;
    const auto root = CreateRef<VerticalBox>();
    manager.SetRoot(root);
    manager.ArrangeLayout({100, 100});
    ASSERT_TRUE(manager.CaptureFrame());
    root->SetVisibility(EVisibility::Hidden);
    EXPECT_FALSE(manager.CaptureFrame());
    root->SetVisibility(EVisibility::Visible);
    EXPECT_TRUE(manager.CaptureFrame());
}
