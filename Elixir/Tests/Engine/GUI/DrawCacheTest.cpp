#include <gtest/gtest.h>
using namespace testing;

#include <algorithm>
#include <limits>

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

    // Leaf that emits `layers` rects at local z 0..layers-1, all tagged with `color`, so a
    // test can locate this widget's commands in the assembled batch and inspect their z range.
    class LayeredWidget final : public Widget
    {
      public:
        LayeredWidget(const SColor& color, const int layers) : m_Color(color), m_Layers(layers) {}

        glm::vec2 ComputeDesiredSize() override { return {}; }

      protected:
        void BuildDrawCommands(RenderBatch& batch, const int zOrder) override
        {
            for (int i = 0; i < m_Layers; ++i)
                batch.AddRect(SRect{}, m_Color, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(0.0f), SOutline{}, zOrder + i);
        }

      private:
        SColor m_Color;
        int m_Layers;
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

TEST(DrawCacheTest, SiblingSubtreesGetDisjointOrderedZBands)
{
    const auto box = CreateRef<VerticalBox>();
    const auto a = CreateRef<LayeredWidget>(SColor(1.0f, 0.0f, 0.0f, 1.0f), 3);  // earlier sibling, local z 0..2
    const auto b = CreateRef<LayeredWidget>(SColor(0.0f, 1.0f, 0.0f, 1.0f), 2);  // later sibling,  local z 0..1
    box->AddChild(a);
    box->AddChild(b);
    Arrange(box, { { 0, 0 }, { 100, 100 } });

    TestGUIManager manager;
    manager.SetRoot(box);
    manager.AssembleFrame();

    int aMax = std::numeric_limits<int>::min();
    int bMin = std::numeric_limits<int>::max();
    for (const auto& cmd : manager.GetRenderBatch().GetCommands())
    {
        if (cmd.Color.R == 1.0f) aMax = std::max(aMax, cmd.ZOrder);   // widget A (red)
        if (cmd.Color.G == 1.0f) bMin = std::min(bMin, cmd.ZOrder);   // widget B (green)
    }

    // The earlier sibling's whole z band must sit strictly below the later sibling's, even
    // though A spans more internal layers than B. This is what stops an earlier child's top
    // layer (e.g. text) from sorting above a later sibling's background.
    EXPECT_LT(aMax, bMin);
}
