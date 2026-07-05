#include <gtest/gtest.h>
using namespace testing;

#include <Engine/GUI/VerticalBox.h>
#include <Engine/GUI/Renderer/RenderBatch.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // Leaf widget that records how many times its draw commands were (re)built.
    class CountingDrawWidget final : public Widget
    {
      public:
        int BuildCount = 0;

        glm::vec2 ComputeDesiredSize() override { return {}; }
        void BuildDrawCommands(RenderBatch& batch, int zOrder) override { ++BuildCount; }
    };

    void Arrange(const Ref<Widget>& widget, const SRect& space)
    {
        widget->ArrangeChildren(space);
    }
}

TEST(DrawCacheTest, CleanWidgetIsNotRebuiltOnSecondCollect)
{
    const auto box = CreateRef<VerticalBox>();
    const auto child = CreateRef<CountingDrawWidget>();
    box->AddChild(child);
    const Ref<Widget> root = box;

    Arrange(root, { { 0, 0 }, { 100, 100 } });

    RenderBatch batch;
    bool rebuilt = false;
    root->CollectDrawCommands(batch, 0, rebuilt);
    EXPECT_EQ(child->BuildCount, 1);
    EXPECT_TRUE(rebuilt);

    // Nothing changed -> the cached commands are reused, no rebuild.
    RenderBatch batch2;
    bool rebuilt2 = false;
    root->CollectDrawCommands(batch2, 0, rebuilt2);
    EXPECT_EQ(child->BuildCount, 1);
    EXPECT_FALSE(rebuilt2);
}

TEST(DrawCacheTest, OnlyTheDirtyWidgetRebuilds)
{
    const auto box = CreateRef<VerticalBox>();
    const auto a = CreateRef<CountingDrawWidget>();
    const auto b = CreateRef<CountingDrawWidget>();
    box->AddChild(a);
    box->AddChild(b);
    const Ref<Widget> root = box;

    Arrange(root, { { 0, 0 }, { 100, 100 } });

    RenderBatch batch;
    bool rebuilt = false;
    root->CollectDrawCommands(batch, 0, rebuilt);
    ASSERT_EQ(a->BuildCount, 1);
    ASSERT_EQ(b->BuildCount, 1);

    // Dirtying only 'a' rebuilds only 'a'; the clean sibling reuses its cache.
    a->MarkRenderDirty();
    RenderBatch batch2;
    bool rebuilt2 = false;
    root->CollectDrawCommands(batch2, 0, rebuilt2);
    EXPECT_EQ(a->BuildCount, 2);
    EXPECT_EQ(b->BuildCount, 1);
    EXPECT_TRUE(rebuilt2);
}
