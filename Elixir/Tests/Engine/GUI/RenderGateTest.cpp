#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <type_traits>
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

TEST(RenderGateTest, PrepareFrameAppliesCurrentPresentation)
{
    const auto box = CreateRef<VerticalBox>();
    box->SetBackgroundColor(EStyleLayer::Normal, SColor(1, 0, 0, 1));
    box->ArrangeChildren({{0, 0}, {100, 100}});
    Manager manager;
    manager.SetRoot(box);

    manager.PrepareFrame();
    ASSERT_FALSE(manager.GetRenderBatch().GetCommands().empty());
    const auto original = manager.GetRenderBatch().GetCommands().front();
    const auto epoch = Widget::CurrentDirtyEpoch();
    manager.PrepareFrame();
    EXPECT_EQ(Widget::CurrentDirtyEpoch(), epoch);

    box->SetOpacity(0.25f);
    box->SetRenderOffset({12, 5});
    manager.PrepareFrame();
    const auto& command = manager.GetRenderBatch().GetCommands().front();
    EXPECT_FLOAT_EQ(command.Color.A, original.Color.A * 0.25f);
    EXPECT_EQ(command.Geometry.Position, original.Geometry.Position + glm::vec2(12, 5));
}

TEST(RenderGateTest, HiddenOrRemovedRootPreparesAnEmptyFrame)
{
    const auto box = CreateRef<VerticalBox>();
    box->SetBackgroundColor(EStyleLayer::Normal, SColor(1, 0, 0, 1));
    box->ArrangeChildren({{0, 0}, {100, 100}});
    Manager manager;
    manager.SetRoot(box);
    manager.PrepareFrame();
    ASSERT_FALSE(manager.GetRenderBatch().GetCommands().empty());

    box->SetVisibility(EVisibility::Collapsed);
    manager.PrepareFrame();
    EXPECT_TRUE(manager.GetRenderBatch().GetCommands().empty());
    box->SetVisibility(EVisibility::Visible);
    manager.PrepareFrame();
    EXPECT_FALSE(manager.GetRenderBatch().GetCommands().empty());
    manager.SetRoot(nullptr);
    manager.PrepareFrame();
    EXPECT_TRUE(manager.GetRenderBatch().GetCommands().empty());
}

namespace
{
    class ThreadCheckingWidget final : public Widget
    {
    public:
        ThreadCheckingWidget() : m_UIThread(std::this_thread::get_id()) {}
        std::atomic<bool> BuiltOnUIThread{true};
        std::atomic<int> BuildCount{0};
        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return {100, 100}; }

    protected:
        void BuildDrawCommands(RenderBatch& batch, int zOrder) override
        {
            if (std::this_thread::get_id() != m_UIThread) BuiltOnUIThread = false;
            ++BuildCount;
            Widget::BuildDrawCommands(batch, zOrder);
        }

    private:
        std::thread::id m_UIThread;
    };
}

TEST(RenderGateTest, RenderDoesNotBuildWidgetsDuringConcurrentPreparation)
{
    const auto box = CreateRef<VerticalBox>();
    const auto child = CreateRef<ThreadCheckingWidget>();
    box->AddChild(child);
    box->ArrangeChildren({{0, 0}, {100, 100}});
    Manager manager;
    manager.SetRoot(box);
    manager.PrepareFrame();
    std::atomic<bool> ready{false};
    std::atomic<bool> done{false};
    std::atomic<int> renderCalls{0};
    std::thread consumer([&]
    {
        ready = true;
        do
        {
            // No GPU renderer is initialized; Render still acquires the published snapshot.
            manager.Render();
            ++renderCalls;
        } while (!done.load());
    });
    while (!ready.load()) std::this_thread::yield();
    for (int i = 0; i < 1000; ++i)
    {
        child->SetBackgroundColor(EStyleLayer::Normal, SColor((i % 10) / 10.0f, 0, 0, 1));
        box->SetRenderOffset({float(i), 0});
        box->SetVisibility(i % 2 ? EVisibility::Visible : EVisibility::Collapsed);
        manager.PrepareFrame();
    }
    done = true;
    consumer.join();
    EXPECT_TRUE(child->BuiltOnUIThread.load());
    EXPECT_GT(child->BuildCount.load(), 1);
    EXPECT_GT(renderCalls.load(), 0);
}

TEST(RenderGateTest, PreparesChangesOnlyWhenPrepareFrameIsCalled)
{
    static_assert(std::is_same_v<decltype(&Manager::PrepareFrame), void (Manager::*)()>);
    static_assert(std::is_same_v<decltype(&Manager::Render), void (Manager::*)()>);

    const auto box = CreateRef<VerticalBox>();
    box->SetBackgroundColor(EStyleLayer::Normal, SColor(1, 0, 0, 1));
    box->ArrangeChildren({{0, 0}, {100, 100}});
    Manager manager;
    manager.SetRoot(box);
    EXPECT_TRUE(manager.GetRenderBatch().GetCommands().empty());
    manager.PrepareFrame();
    EXPECT_FALSE(manager.GetRenderBatch().GetCommands().empty());

    box->SetVisibility(EVisibility::Collapsed);
    EXPECT_FALSE(manager.GetRenderBatch().GetCommands().empty());
    manager.PrepareFrame();
    EXPECT_TRUE(manager.GetRenderBatch().GetCommands().empty());
}
