#include <gtest/gtest.h>
using namespace testing;

#include "ManagerTestUtils.h"

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

        using Widget::MarkRenderDirty;

    protected:
        void BuildDrawCommands(RenderBatch& batch, int zOrder) override { ++BuildCount; }
    };

    void Arrange(const Ref<Widget>& widget, const SRect& space)
    {
        widget->ArrangeChildren(space);
    }

    void AssembleFrame(const Ref<Panel>& panel)
    {
        TestGUIManager manager;
        manager.SetRoot(panel);
        manager.AssembleFrame();
    }
}

TEST(DrawCacheTest, CleanWidgetIsNotRebuiltOnSecondCollect)
{
    const auto box = CreateRef<VerticalBox>();
    const auto child = CreateRef<CountingDrawWidget>();
    box->AddChild(child);

    Arrange(box, { { 0, 0 }, { 100, 100 } });

    AssembleFrame(box);
    EXPECT_EQ(child->BuildCount, 1);

    // Nothing changed -> the cached commands are reused, no rebuild.
    AssembleFrame(box);
    EXPECT_EQ(child->BuildCount, 1);
}

TEST(DrawCacheTest, OnlyTheDirtyWidgetRebuilds)
{
    const auto box = CreateRef<VerticalBox>();
    const auto a = CreateRef<CountingDrawWidget>();
    const auto b = CreateRef<CountingDrawWidget>();
    box->AddChild(a);
    box->AddChild(b);

    Arrange(box, { { 0, 0 }, { 100, 100 } });

    AssembleFrame(box);
    ASSERT_EQ(a->BuildCount, 1);
    ASSERT_EQ(b->BuildCount, 1);

    // Dirtying only 'a' rebuilds only 'a'; the clean sibling reuses its cache.
    a->MarkRenderDirty();
    AssembleFrame(box);
    EXPECT_EQ(a->BuildCount, 2);
    EXPECT_EQ(b->BuildCount, 1);
}

TEST(DrawCacheTest, GeometryChangeRebuildsCache)
{
    const auto box = CreateRef<VerticalBox>();
    const auto child = CreateRef<CountingDrawWidget>();
    box->SetStretching(true);   // the child width tracks the box width
    box->AddChild(child);

    Arrange(box, { { 0, 0 }, { 100, 100 } });
    AssembleFrame(box);
    ASSERT_EQ(child->BuildCount, 1);

    // Nothing changed -> cache reused.
    AssembleFrame(box);
    ASSERT_EQ(child->BuildCount, 1);

    // Re-arranging at a wider rect resizes the child; its cache was built at the old
    // geometry, so it must rebuild even though no visual setter was touched.
    Arrange(box, { { 0, 0 }, { 200, 200 } });
    AssembleFrame(box);
    EXPECT_EQ(child->BuildCount, 2);
}
