#include <gtest/gtest.h>
#include <atomic>
#include <thread>
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

TEST(RenderGateTest, CapturedFrameKeepsItsCommandsWhenWidgetsChange)
{
    const auto box = CreateRef<VerticalBox>();
    box->SetBackgroundColor(EStyleLayer::Normal, SColor(1, 0, 0, 1));
    box->ArrangeChildren({{0, 0}, {100, 100}});
    TestGUIManager manager;
    manager.SetRoot(box);

    const auto first = manager.CaptureRenderFrame();
    ASSERT_FALSE(first->GetCommands().empty());
    const auto original = first->GetCommands().front();
    EXPECT_EQ(manager.CaptureRenderFrame(), first);

    box->SetOpacity(0.25f);
    box->SetRenderOffset({12, 5});
    const auto second = manager.CaptureRenderFrame();
    ASSERT_NE(second, first);
    EXPECT_EQ(first->GetCommands().front().Color.A, original.Color.A);
    EXPECT_EQ(first->GetCommands().front().Geometry.Position, original.Geometry.Position);
    EXPECT_FLOAT_EQ(second->GetCommands().front().Color.A, original.Color.A * 0.25f);
    EXPECT_EQ(second->GetCommands().front().Geometry.Position, original.Geometry.Position + glm::vec2(12, 5));
}

TEST(RenderGateTest, HiddenOrRemovedRootPublishesAnEmptyFrame)
{
    const auto box = CreateRef<VerticalBox>();
    box->SetBackgroundColor(EStyleLayer::Normal, SColor(1, 0, 0, 1));
    box->ArrangeChildren({{0, 0}, {100, 100}});
    TestGUIManager manager;
    manager.SetRoot(box);
    const auto visible = manager.CaptureRenderFrame();
    ASSERT_FALSE(visible->GetCommands().empty());

    box->SetVisibility(EVisibility::Collapsed);
    EXPECT_TRUE(manager.CaptureRenderFrame()->GetCommands().empty());
    box->SetVisibility(EVisibility::Visible);
    EXPECT_FALSE(manager.CaptureRenderFrame()->GetCommands().empty());
    manager.SetRoot(nullptr);
    EXPECT_TRUE(manager.CaptureRenderFrame()->GetCommands().empty());
    EXPECT_FALSE(visible->GetCommands().empty());
}

TEST(RenderGateTest, RenderThreadCanRetainOldFramesDuringPanelTransitions)
{
    const auto box = CreateRef<VerticalBox>();
    box->SetBackgroundColor(EStyleLayer::Normal, SColor(1, 0, 0, 1));
    box->ArrangeChildren({{0, 0}, {100, 100}});
    TestGUIManager manager;
    manager.SetRoot(box);
    const auto frame = manager.CaptureRenderFrame();
    ASSERT_FALSE(frame->GetCommands().empty());
    const auto original = frame->GetCommands().front();
    std::atomic<bool> valid{true};
    std::atomic<bool> ready{false};
    std::atomic<bool> done{false};
    std::thread consumer([&]
    {
        ready = true;
        while (!done.load())
        {
            const auto& command = frame->GetCommands().front();
            if (command.Color.A != original.Color.A || command.Geometry.Position != original.Geometry.Position)
                valid = false;
            // With no GPU renderer, this still exercises synchronized publication reads.
            manager.Render();
            manager.Render(frame);
        }
    });
    while (!ready.load()) std::this_thread::yield();
    for (int i = 0; i < 1000; ++i)
    {
        box->SetOpacity((i % 10) / 10.0f);
        box->SetRenderOffset({float(i), 0});
        box->SetVisibility(i % 2 ? EVisibility::Visible : EVisibility::Collapsed);
        manager.CaptureRenderFrame();
    }
    done = true;
    consumer.join();
    EXPECT_TRUE(valid.load());
}
