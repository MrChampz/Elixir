#include <gtest/gtest.h>
using namespace testing;

#include "ManagerTestUtils.h"

#include <Engine/GUI/ScrollBox.h>
#include <Engine/GUI/VerticalBox.h>
#include <Engine/GUI/Renderer/RenderBatch.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // Leaf that emits a single rect tagged with `color` at its own geometry, with no ad-hoc
    // ScissorRect of its own - the default AddRect scissor sentinel {-1,-1}/{-1,-1} (see
    // RenderBatch::AddRect) - so a test can tell purely-inherited clip from a self-clip.
    class ProbeWidget final : public Widget
    {
      public:
        explicit ProbeWidget(const SColor& color, const glm::vec2& desired = { 20.0f, 20.0f })
            : m_Color(color), m_Desired(desired) {}

        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return m_Desired; }

      protected:
        void BuildDrawCommands(RenderBatch& batch, const int zOrder) override
        {
            batch.AddRect(m_Geometry, m_Color, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(0.0f), SOutline{}, zOrder);
        }

      private:
        SColor m_Color;
        glm::vec2 m_Desired;
    };

    // Same as ProbeWidget, but emits its own ad-hoc self-clip scissor - the Button/TextField
    // pattern of clipping their own label to their own bounds - so a test can check that an
    // inherited clip narrows it further instead of replacing it.
    class SelfClippingProbeWidget final : public Widget
    {
      public:
        explicit SelfClippingProbeWidget(const SColor& color) : m_Color(color) {}

        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return { 20.0f, 20.0f }; }

      protected:
        void BuildDrawCommands(RenderBatch& batch, const int zOrder) override
        {
            batch.AddRect(m_Geometry, m_Color, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(0.0f), SOutline{}, zOrder, m_Geometry);
        }

      private:
        SColor m_Color;
    };

    // Minimal container that clips its children to its own geometry, without any of
    // ScrollBox's scrolling/scrollbar machinery - isolates clip propagation itself. Deliberately
    // does not lay out its children (Panel/TPanel provide no default layout of their own -
    // VerticalBox is the one that positions slots): tests that care about a child's own
    // geometry arrange it directly, so a child's geometry here is always independent of the
    // box's, which is exactly what the self-clip-vs-inherited-clip test needs.
    class ClippingBox final : public TPanel<LayoutSlot>
    {
      public:
        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return {}; }

      protected:
        bool ClipsChildren() const override { return true; }
    };

    void Arrange(const Ref<Widget>& widget, const SRect& space)
    {
        widget->ArrangeChildren(space);
    }

    // Finds the command tagged with `color`; nullptr if none matched. Callers ASSERT_NE the
    // result before dereferencing, so a missing probe fails with a clear gtest message instead
    // of a null-deref.
    const SDrawCommand* FindByColor(const std::vector<SDrawCommand>& commands, const SColor& color)
    {
        for (const auto& cmd : commands)
        {
            if (cmd.Color == color)
                return &cmd;
        }
        return nullptr;
    }
}

TEST(ClipStackTest, NonClippingContainerLeavesChildScissorInvalid)
{
    const auto box = CreateRef<VerticalBox>();
    const SColor probeColor(1.0f, 0.0f, 0.0f, 1.0f);
    const auto probe = CreateRef<ProbeWidget>(probeColor);
    box->AddChild(probe);

    Arrange(box, { { 0, 0 }, { 100, 100 } });

    TestGUIManager manager;
    manager.SetRoot(box);
    manager.AssembleFrame();

    const auto* cmd = FindByColor(manager.GetRenderBatch().GetCommands(), probeColor);
    ASSERT_NE(cmd, nullptr);
    EXPECT_FALSE(cmd->ScissorRect.IsValid());
}

TEST(ClipStackTest, ClippingContainerGivesChildScissorEqualToItsGeometry)
{
    const auto box = CreateRef<ClippingBox>();
    const SColor probeColor(0.0f, 1.0f, 0.0f, 1.0f);
    const auto probe = CreateRef<ProbeWidget>(probeColor);
    box->AddChild(probe);

    Arrange(box, { { 5, 5 }, { 50, 50 } });

    TestGUIManager manager;
    manager.SetRoot(box);
    manager.AssembleFrame();

    const auto* cmd = FindByColor(manager.GetRenderBatch().GetCommands(), probeColor);
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->ScissorRect.IsValid());
    EXPECT_EQ(cmd->ScissorRect.Position, box->GetGeometry().Position);
    EXPECT_EQ(cmd->ScissorRect.Size, box->GetGeometry().Size);
}

TEST(ClipStackTest, NestedClipsIntersectRatherThanReplace)
{
    // Outer clips to a big rect, inner (offset, smaller) clips further -> the child's final
    // scissor must be the intersection, strictly smaller than the outer clip alone.
    const auto outer = CreateRef<ClippingBox>();
    const auto inner = CreateRef<ClippingBox>();
    const SColor probeColor(0.0f, 0.0f, 1.0f, 1.0f);
    const auto probe = CreateRef<ProbeWidget>(probeColor);

    outer->AddChild(inner);
    inner->AddChild(probe);

    Arrange(outer, { { 0, 0 }, { 100, 100 } });
    inner->ArrangeChildren({ { 20, 20 }, { 30, 30 } });

    TestGUIManager manager;
    manager.SetRoot(outer);
    manager.AssembleFrame();

    const auto* cmd = FindByColor(manager.GetRenderBatch().GetCommands(), probeColor);
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->ScissorRect.IsValid());

    const SRect expected = SRect::Intersect(outer->GetGeometry(), inner->GetGeometry());
    EXPECT_EQ(cmd->ScissorRect.Position, expected.Position);
    EXPECT_EQ(cmd->ScissorRect.Size, expected.Size);

    // Sanity: the intersection is genuinely smaller than the outer clip alone, otherwise this
    // test would pass even if nesting silently replaced instead of intersecting.
    EXPECT_LT(cmd->ScissorRect.Size.x, outer->GetGeometry().Size.x);
    EXPECT_LT(cmd->ScissorRect.Size.y, outer->GetGeometry().Size.y);
}

TEST(ClipStackTest, InheritedClipNarrowsExistingSelfClipInsteadOfReplacingIt)
{
    const auto box = CreateRef<ClippingBox>();
    const SColor probeColor(1.0f, 1.0f, 0.0f, 1.0f);
    const auto probe = CreateRef<SelfClippingProbeWidget>(probeColor);
    box->AddChild(probe);

    // ClippingBox never re-arranges its children (see its comment above), so the probe's own
    // geometry - and therefore its self-clip - is set here directly and stays fixed regardless
    // of what happens to the box afterwards; only the box's own geometry (the inherited clip)
    // changes below.
    Arrange(box, { { 0, 0 }, { 100, 100 } });
    probe->ArrangeChildren({ { 0, 0 }, { 40, 40 } });

    TestGUIManager manager;
    manager.SetRoot(box);
    manager.AssembleFrame();

    // Box is bigger than the probe -> self-clip == probe geometry; intersecting with the
    // inherited (larger) box clip should leave it exactly at the probe's own geometry.
    const auto* cmd = FindByColor(manager.GetRenderBatch().GetCommands(), probeColor);
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->ScissorRect.IsValid());
    EXPECT_EQ(cmd->ScissorRect.Position, probe->GetGeometry().Position);
    EXPECT_EQ(cmd->ScissorRect.Size, probe->GetGeometry().Size);

    // Now narrow the box below the probe's own geometry: the inherited clip must actually
    // shrink the self-clip, not be discarded in favor of it.
    box->ArrangeChildren({ { 0, 0 }, { 5, 5 } });
    manager.AssembleFrame();

    const auto* cmd2 = FindByColor(manager.GetRenderBatch().GetCommands(), probeColor);
    ASSERT_NE(cmd2, nullptr);
    ASSERT_TRUE(cmd2->ScissorRect.IsValid());
    EXPECT_EQ(cmd2->ScissorRect.Size.x, 5.0f);
    EXPECT_EQ(cmd2->ScissorRect.Size.y, 5.0f);
}

// Direct regression test for a real bug: ScrollBox::ClipsChildren() once returned false by
// mistake, and nothing caught it. Pin both halves of the contract: the flag itself, and the
// end-to-end effect (oversized content actually gets a non-empty ScissorRect after a frame).
TEST(ClipStackTest, ScrollBoxClipsChildrenIsTrueAndOversizedContentGetsScissored)
{
    // Manager::SetRoot takes a Panel; ScrollBox is a ContentWidget, not a Panel, so it needs
    // a trivial Panel host - a VerticalBox filling the same rect - the same way any real UI
    // would embed a ScrollBox inside a layout container.
    const auto root = CreateRef<VerticalBox>();
    const auto scrollBox = CreateRef<ScrollBox>();
    scrollBox->SetDesiredSize({ 50.0f, 50.0f });
    root->AddChild(scrollBox).SetHorizontalAlignment(EHorizontalAlignment::Fill)
                              .SetVerticalAlignment(EVerticalAlignment::Fill);

    const SColor probeColor(1.0f, 0.0f, 1.0f, 1.0f);
    const auto content = CreateRef<ProbeWidget>(probeColor, glm::vec2{ 200.0f, 200.0f }); // larger than the viewport
    scrollBox->SetContent(content);

    Arrange(root, { { 0, 0 }, { 50, 50 } });

    TestGUIManager manager;
    manager.SetRoot(root);
    manager.AssembleFrame();

    const auto* cmd = FindByColor(manager.GetRenderBatch().GetCommands(), probeColor);
    ASSERT_NE(cmd, nullptr);
    ASSERT_TRUE(cmd->ScissorRect.IsValid()) << "ScrollBox::ClipsChildren() must clip its content";
    EXPECT_GT(cmd->ScissorRect.Size.x, 0.0f);
    EXPECT_GT(cmd->ScissorRect.Size.y, 0.0f);
    EXPECT_EQ(cmd->ScissorRect.Size.x, scrollBox->GetGeometry().Size.x);
    EXPECT_EQ(cmd->ScissorRect.Size.y, scrollBox->GetGeometry().Size.y);
}
