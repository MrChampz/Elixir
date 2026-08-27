#include <gtest/gtest.h>
using namespace testing;

#include <Engine/GUI/Canvas.h>
#include <Engine/GUI/Manager.h>
#include <Engine/GUI/Renderer/RenderBatch.h>
#include <Engine/GUI/ScrollBox.h>
#include <Engine/Input/InputManager.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // Leaf widget whose desired size can be driven from the test - same pattern as
    // WidgetTestUtils.h's CountingWidget, without pulling in its ArrangeCount bookkeeping.
    class SizedLeaf final : public Widget
    {
      public:
        explicit SizedLeaf(const glm::vec2& desired) : m_Desired(desired) {}

        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return m_Desired; }

        void SetDesiredSize(const glm::vec2& desired)
        {
            if (m_Desired == desired) return;
            m_Desired = desired;
            MarkLayoutDirty();
        }

      private:
        glm::vec2 m_Desired;
    };

    class WidthDependentLeaf final : public Widget
    {
      protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override
        {
            return { availableSize.x, availableSize.x > 200.0f ? 20.0f : 100.0f };
        }
    };

    class GutterSensitiveLeaf final : public Widget
    {
      protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override
        {
            return availableSize.x < 100.0f
                ? glm::vec2{ availableSize.x, 200.0f }
                : glm::vec2{ availableSize.x, 20.0f };
        }
    };

    // ScrollBox's own promoted surface: HandleMouseScrolled and ClipsChildren are protected
    // overrides with no public equivalent, so this test double promotes them the same way
    // ForEachChildTest.cpp/WidgetLifetimeTest.cpp promote other protected members.
    class TestScrollBox final : public ScrollBox
    {
      public:
        using ScrollBox::ClipsChildren;
        using ScrollBox::BuildDrawCommands;
        using ScrollBox::CollectDrawCommands;
        using ScrollBox::HandleMouseScrolled;
    };

    void Arrange(const Ref<Widget>& widget, const SRect& space)
    {
        widget->ArrangeChildren(space);
    }

    void BuildDrawCache(const Ref<TestScrollBox>& widget)
    {
        RenderBatch batch;
        int zOrder = 0;
        bool rebuilt = false;
        widget->CollectDrawCommands(batch, zOrder, rebuilt, {{ -1, -1 }, { -1, -1 }});
    }
}

TEST(ScrollBoxTest, DesiredSizeNeverExceedsViewportEvenWithLargerContent)
{
    const auto scrollBox = CreateRef<ScrollBox>();
    scrollBox->SetSize({ 50.0f, 50.0f });
    scrollBox->SetContent(CreateRef<SizedLeaf>(glm::vec2{ 500.0f, 500.0f }));

    // GetDesiredSize() reads Measure()'s cache; ArrangeChildren alone never populates it
    // (that's the parent's job when it measures this widget before arranging it), so the
    // test has to call Measure directly, same as any real container would.
    const glm::vec2 desired = scrollBox->Measure({ 1000.0f, 1000.0f });
    EXPECT_LE(desired.x, 50.0f);
    EXPECT_LE(desired.y, 50.0f);
}

TEST(ScrollBoxTest, DesiredSizeShrinksToContentWhenContentIsSmallerThanViewport)
{
    const auto scrollBox = CreateRef<ScrollBox>();
    scrollBox->SetSize({ 100.0f, 100.0f });
    scrollBox->SetContent(CreateRef<SizedLeaf>(glm::vec2{ 30.0f, 20.0f }));

    const glm::vec2 desired = scrollBox->Measure({ 1000.0f, 1000.0f });
    EXPECT_EQ(desired.x, 30.0f);
    EXPECT_EQ(desired.y, 20.0f);
}

TEST(ScrollBoxTest, ContentThatFitsDoesNotReserveAScrollbarGutter)
{
    const auto scrollBox = CreateRef<TestScrollBox>();
    scrollBox->SetSize({ 100.0f, 100.0f });
    const auto content = CreateRef<GutterSensitiveLeaf>();
    scrollBox->SetContent(content);

    const glm::vec2 desired = scrollBox->Measure({ 1000.0f, 1000.0f });
    EXPECT_EQ(desired, glm::vec2(100.0f, 20.0f));

    Arrange(scrollBox, { { 0.0f, 0.0f }, desired });
    EXPECT_EQ(content->GetGeometry().Size.x, 100.0f);

    RenderBatch batch;
    scrollBox->BuildDrawCommands(batch, 0);
    EXPECT_TRUE(batch.GetCommands().empty());
}

TEST(ScrollBoxTest, DesiredSizeMeasuresContentAgainstTheConfiguredViewport)
{
    const auto scrollBox = CreateRef<ScrollBox>();
    scrollBox->SetSize({ 200.0f, 200.0f });
    scrollBox->SetContent(CreateRef<WidthDependentLeaf>());

    const glm::vec2 desired = scrollBox->Measure({ 1000.0f, 1000.0f });

    EXPECT_EQ(desired.y, 100.0f);
}

TEST(ScrollBoxTest, DesiredSizePreservesTheActiveScrollbarGutter)
{
    const auto scrollBox = CreateRef<ScrollBox>();
    scrollBox->SetSize({ 100.0f, 100.0f });
    const auto content = CreateRef<SizedLeaf>(glm::vec2{ 30.0f, 200.0f });
    scrollBox->SetContent(content);

    // The vertical scrollbar needs 8 pixels, so a 30-pixel-wide child requires a
    // 38-pixel-wide ScrollBox. The child then receives its full 30-pixel width.
    const glm::vec2 desired = scrollBox->Measure({ 1000.0f, 1000.0f });
    EXPECT_EQ(desired.x, 38.0f);
    EXPECT_EQ(desired.y, 100.0f);

    Arrange(scrollBox, { { 0.0f, 0.0f }, desired });
    EXPECT_EQ(content->GetGeometry().Size.x, 30.0f);
}

TEST(ScrollBoxTest, LayoutChildrenOffsetsContentByCurrentScrollOffset)
{
    const auto scrollBox = CreateRef<ScrollBox>();
    scrollBox->SetSize({ 50.0f, 50.0f });
    scrollBox->SetScrollAxis(EScrollAxis::Both); // both axes must scroll for this to move X too
    const auto content = CreateRef<SizedLeaf>(glm::vec2{ 200.0f, 200.0f });
    scrollBox->SetContent(content);

    Arrange(scrollBox, { { 0, 0 }, { 50, 50 } });
    const glm::vec2 unscrolledPos = content->GetGeometry().Position;

    scrollBox->SetScrollOffset({ 10.0f, 15.0f });
    Arrange(scrollBox, { { 0, 0 }, { 50, 50 } }); // same rect, but offset changed -> not skipped

    const glm::vec2 scrolledPos = content->GetGeometry().Position;
    EXPECT_EQ(scrolledPos.x, unscrolledPos.x - 10.0f);
    EXPECT_EQ(scrolledPos.y, unscrolledPos.y - 15.0f);
}

TEST(ScrollBoxTest, SetScrollOffsetClampsAboveMaxAndBelowZero)
{
    const auto scrollBox = CreateRef<ScrollBox>();
    scrollBox->SetSize({ 50.0f, 50.0f });
    scrollBox->SetScrollAxis(EScrollAxis::Both); // both axes must scroll to clamp X too
    scrollBox->SetContent(CreateRef<SizedLeaf>(glm::vec2{ 200.0f, 150.0f }));

    Arrange(scrollBox, { { 0, 0 }, { 50, 50 } }); // establishes m_ContentSize used to clamp

    // Both scrollbars reserve 8 pixels, leaving a (42,42) content viewport. The final
    // (8-pixel) gutter remains reachable rather than being clipped at (150,100).
    scrollBox->SetScrollOffset({ 9999.0f, 9999.0f });
    EXPECT_EQ(scrollBox->GetScrollOffset().x, 158.0f);
    EXPECT_EQ(scrollBox->GetScrollOffset().y, 108.0f);

    // Below zero clamps to zero.
    scrollBox->SetScrollOffset({ -50.0f, -50.0f });
    EXPECT_EQ(scrollBox->GetScrollOffset().x, 0.0f);
    EXPECT_EQ(scrollBox->GetScrollOffset().y, 0.0f);
}

TEST(ScrollBoxTest, BothAxisScrollbarsUseTheGutterReducedViewport)
{
    const auto scrollBox = CreateRef<TestScrollBox>();
    scrollBox->SetSize({ 50.0f, 50.0f });
    scrollBox->SetScrollAxis(EScrollAxis::Both);
    scrollBox->SetContent(CreateRef<SizedLeaf>(glm::vec2{ 200.0f, 150.0f }));
    Arrange(scrollBox, { { 0.0f, 0.0f }, { 50.0f, 50.0f } });
    scrollBox->SetScrollOffset({ 9999.0f, 9999.0f });

    RenderBatch batch;
    scrollBox->BuildDrawCommands(batch, 0);

    const auto& commands = batch.GetCommands();
    ASSERT_EQ(commands.size(), 4u);

    const SRect& verticalTrack = commands[0].Geometry;
    const SRect& verticalThumb = commands[1].Geometry;
    const SRect& horizontalTrack = commands[2].Geometry;
    const SRect& horizontalThumb = commands[3].Geometry;

    EXPECT_EQ(verticalTrack.Position.x, 42.0f);
    EXPECT_EQ(verticalTrack.Size.y, 42.0f);
    EXPECT_EQ(horizontalTrack.Position.y, 42.0f);
    EXPECT_EQ(horizontalTrack.Size.x, 42.0f);
    EXPECT_FLOAT_EQ(verticalThumb.Position.y + verticalThumb.Size.y, 42.0f);
    EXPECT_FLOAT_EQ(horizontalThumb.Position.x + horizontalThumb.Size.x, 42.0f);
}

TEST(ScrollBoxTest, BothAxisScrollbarsAccountForGuttersIntroducedByEachOther)
{
    const auto scrollBox = CreateRef<TestScrollBox>();
    scrollBox->SetSize({ 100.0f, 100.0f });
    scrollBox->SetScrollAxis(EScrollAxis::Both);
    scrollBox->SetContent(CreateRef<SizedLeaf>(glm::vec2{ 95.0f, 200.0f }));
    Arrange(scrollBox, { { 0.0f, 0.0f }, { 100.0f, 100.0f } });

    RenderBatch batch;
    scrollBox->BuildDrawCommands(batch, 0);

    const auto& commands = batch.GetCommands();
    ASSERT_EQ(commands.size(), 4u);
    EXPECT_EQ(commands[0].Geometry.Size.y, 92.0f);
    EXPECT_EQ(commands[2].Geometry.Size.x, 92.0f);
}

TEST(ScrollBoxTest, LayoutInvalidatesRenderingWhenContentSizeChanges)
{
    const auto scrollBox = CreateRef<TestScrollBox>();
    scrollBox->SetSize({ 50.0f, 50.0f });
    const auto content = CreateRef<SizedLeaf>(glm::vec2{ 50.0f, 200.0f });
    scrollBox->SetContent(content);
    Arrange(scrollBox, { { 0.0f, 0.0f }, { 50.0f, 50.0f } });

    BuildDrawCache(scrollBox);
    ASSERT_FALSE(scrollBox->IsRenderDirty());

    content->SetDesiredSize({ 50.0f, 100.0f });
    Arrange(scrollBox, { { 0.0f, 0.0f }, { 50.0f, 50.0f } });

    EXPECT_TRUE(scrollBox->IsRenderDirty());
}

TEST(ScrollBoxTest, ChangingScrollAxisInvalidatesRendering)
{
    const auto scrollBox = CreateRef<TestScrollBox>();
    scrollBox->SetSize({ 50.0f, 50.0f });
    scrollBox->SetContent(CreateRef<SizedLeaf>(glm::vec2{ 200.0f, 200.0f }));
    Arrange(scrollBox, { { 0.0f, 0.0f }, { 50.0f, 50.0f } });

    BuildDrawCache(scrollBox);
    ASSERT_FALSE(scrollBox->IsRenderDirty());

    scrollBox->SetScrollAxis(EScrollAxis::Horizontal);

    EXPECT_TRUE(scrollBox->IsRenderDirty());
}

TEST(ScrollBoxTest, ChangingScrollbarVisibilityInvalidatesLayout)
{
    const auto scrollBox = CreateRef<TestScrollBox>();
    scrollBox->SetSize({ 50.0f, 50.0f });
    scrollBox->SetContent(CreateRef<SizedLeaf>(glm::vec2{ 200.0f, 200.0f }));
    Arrange(scrollBox, { { 0.0f, 0.0f }, { 50.0f, 50.0f } });

    ASSERT_FALSE(scrollBox->IsLayoutDirty());
    scrollBox->SetShowScrollbar(false);

    EXPECT_TRUE(scrollBox->IsLayoutDirty());
    EXPECT_TRUE(scrollBox->IsRenderDirty());
}

TEST(ScrollBoxTest, HandleMouseScrolledMovesOffsetWithinBoundsAndReportsHandled)
{
    const auto scrollBox = CreateRef<TestScrollBox>();
    scrollBox->SetSize({ 50.0f, 50.0f });
    scrollBox->SetContent(CreateRef<SizedLeaf>(glm::vec2{ 200.0f, 200.0f }));
    Arrange(scrollBox, { { 0, 0 }, { 50, 50 } });

    ASSERT_EQ(scrollBox->GetScrollOffset().y, 0.0f);

    // Positive wheel offset scrolls content up (offset increases), per ScrollBox's
    // delta.y = -event.GetOffsetY() * SCROLL_SPEED convention.
    const MouseScrolledEvent scrollDown(0.0f, -1.0f);
    const SInputReply reply = scrollBox->HandleMouseScrolled(scrollDown);

    EXPECT_TRUE(reply.EventHandled);
    EXPECT_GT(scrollBox->GetScrollOffset().y, 0.0f);
}

TEST(ScrollBoxTest, HandleMouseScrolledAtEdgeIsUnhandledSoAnAncestorCanTry)
{
    const auto scrollBox = CreateRef<TestScrollBox>();
    scrollBox->SetSize({ 50.0f, 50.0f });
    // Content fits entirely within the viewport -> already at both scroll edges (offset 0,
    // max offset 0), so any wheel delta must be rejected.
    scrollBox->SetContent(CreateRef<SizedLeaf>(glm::vec2{ 20.0f, 20.0f }));
    Arrange(scrollBox, { { 0, 0 }, { 50, 50 } });

    const MouseScrolledEvent scrollDown(0.0f, -1.0f);
    const SInputReply reply = scrollBox->HandleMouseScrolled(scrollDown);

    EXPECT_FALSE(reply.EventHandled);
    EXPECT_EQ(scrollBox->GetScrollOffset().y, 0.0f);
}

TEST(ScrollBoxTest, WheelEventUsesTheCurrentPointerHitPath)
{
    const auto root = CreateRef<Canvas>();
    root->SetSize({ 200.0f, 100.0f });

    const auto left = CreateRef<ScrollBox>();
    left->SetSize({ 100.0f, 100.0f });
    left->SetContent(CreateRef<SizedLeaf>(glm::vec2{ 100.0f, 200.0f }));
    root->AddChild(left).SetSize({ 100.0f, 100.0f });

    const auto right = CreateRef<ScrollBox>();
    right->SetSize({ 100.0f, 100.0f });
    right->SetContent(CreateRef<SizedLeaf>(glm::vec2{ 100.0f, 200.0f }));
    root->AddChild(right).SetPosition({ 100.0f, 0.0f }).SetSize({ 100.0f, 100.0f });

    root->ArrangeChildren({ { 0.0f, 0.0f }, { 200.0f, 100.0f } });

    Manager manager;
    manager.SetRoot(root);

    MouseMovedEvent moved(150.0f, 50.0f);
    InputManager::OnEvent(moved);

    MouseScrolledEvent scrollDown(0.0f, -1.0f);
    manager.ProcessEvent(scrollDown);

    EXPECT_EQ(left->GetScrollOffset().y, 0.0f);
    EXPECT_GT(right->GetScrollOffset().y, 0.0f);
}

TEST(ScrollBoxTest, ClipsChildrenIsTrue)
{
    const auto scrollBox = CreateRef<TestScrollBox>();
    EXPECT_TRUE(scrollBox->ClipsChildren());
}

TEST(ScrollBarStyleTest, FallsBackToNormalAndKeepsSizingMetrics)
{
    SScrollBarStyle style;
    style.Thickness = 12.0f;
    style.MinimumThumbLength = 20.0f;
    style.Normal.Thumb.Color = { 0.3f, 0.4f, 0.5f, 1.0f };

    EXPECT_EQ(style.Thickness, 12.0f);
    EXPECT_EQ(style.MinimumThumbLength, 20.0f);
    EXPECT_EQ(style.Get(EStyleLayer::Hovered).Thumb.Color, SColor(0.3f, 0.4f, 0.5f, 1.0f));
}

TEST(ScrollBoxTest, SetStyleUpdatesExposedScrollbarProperties)
{
    const auto scrollBox = CreateRef<ScrollBox>();

    SScrollBarStyle style;
    style.Thickness = 12.0f;
    style.MinimumThumbLength = 20.0f;
    style.Normal.Thumb.Color = { 0.3f, 0.4f, 0.5f, 1.0f };
    scrollBox->SetStyle(style);

    EXPECT_EQ(scrollBox->GetScrollbarThickness(), 12.0f);
    EXPECT_EQ(scrollBox->GetScrollbarColor(), SColor(0.3f, 0.4f, 0.5f, 1.0f));
}

TEST(ScrollBoxTest, MinimumThumbLengthNeverExceedsTheTrack)
{
    const auto scrollBox = CreateRef<TestScrollBox>();
    scrollBox->SetSize({ 20.0f, 20.0f });
    scrollBox->SetScrollAxis(EScrollAxis::Both);
    scrollBox->SetContent(CreateRef<SizedLeaf>(glm::vec2{ 200.0f, 200.0f }));

    SScrollBarStyle style;
    style.MinimumThumbLength = 100.0f;
    scrollBox->SetStyle(style);

    Arrange(scrollBox, { { 0.0f, 0.0f }, { 20.0f, 20.0f } });
    scrollBox->SetScrollOffset({ 9999.0f, 9999.0f });

    RenderBatch batch;
    scrollBox->BuildDrawCommands(batch, 0);
    const auto& commands = batch.GetCommands();
    ASSERT_EQ(commands.size(), 4u);

    const SRect& verticalTrack = commands[0].Geometry;
    const SRect& verticalThumb = commands[1].Geometry;
    const SRect& horizontalTrack = commands[2].Geometry;
    const SRect& horizontalThumb = commands[3].Geometry;

    EXPECT_LE(verticalThumb.Size.y, verticalTrack.Size.y);
    EXPECT_LE(verticalThumb.Position.y + verticalThumb.Size.y, verticalTrack.Position.y + verticalTrack.Size.y);
    EXPECT_LE(horizontalThumb.Size.x, horizontalTrack.Size.x);
    EXPECT_LE(horizontalThumb.Position.x + horizontalThumb.Size.x, horizontalTrack.Position.x + horizontalTrack.Size.x);
}

TEST(ScrollBoxTest, HitTestExcludesScrolledContentOutsideTheViewport)
{
    const auto scrollBox = CreateRef<ScrollBox>();
    scrollBox->SetSize({ 50.0f, 50.0f });
    const auto content = CreateRef<SizedLeaf>(glm::vec2{ 50.0f, 200.0f });
    scrollBox->SetContent(content);

    Arrange(scrollBox, { { 0.0f, 28.0f }, { 50.0f, 50.0f } });
    scrollBox->SetScrollOffset({ 0.0f, 20.0f });
    Arrange(scrollBox, { { 0.0f, 28.0f }, { 50.0f, 50.0f } });

    std::vector<Ref<Widget>> path;
    scrollBox->HitTest({ 10.0f, 10.0f }, path);

    EXPECT_TRUE(path.empty());
}
