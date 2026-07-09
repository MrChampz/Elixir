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
