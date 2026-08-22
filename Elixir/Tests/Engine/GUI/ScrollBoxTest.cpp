#include <gtest/gtest.h>
using namespace testing;

#include <Engine/GUI/ScrollBox.h>
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

      private:
        glm::vec2 m_Desired;
    };

    // ScrollBox's own promoted surface: HandleMouseScrolled and ClipsChildren are protected
    // overrides with no public equivalent, so this test double promotes them the same way
    // ForEachChildTest.cpp/WidgetLifetimeTest.cpp promote other protected members.
    class TestScrollBox final : public ScrollBox
    {
      public:
        using ScrollBox::ClipsChildren;
        using ScrollBox::HandleMouseScrolled;
    };

    void Arrange(const Ref<Widget>& widget, const SRect& space)
    {
        widget->ArrangeChildren(space);
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

    // Above the max: content (200,150) minus viewport (50,50) = (150,100) max scroll.
    scrollBox->SetScrollOffset({ 9999.0f, 9999.0f });
    EXPECT_EQ(scrollBox->GetScrollOffset().x, 150.0f);
    EXPECT_EQ(scrollBox->GetScrollOffset().y, 100.0f);

    // Below zero clamps to zero.
    scrollBox->SetScrollOffset({ -50.0f, -50.0f });
    EXPECT_EQ(scrollBox->GetScrollOffset().x, 0.0f);
    EXPECT_EQ(scrollBox->GetScrollOffset().y, 0.0f);
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

TEST(ScrollBoxTest, ClipsChildrenIsTrue)
{
    const auto scrollBox = CreateRef<TestScrollBox>();
    EXPECT_TRUE(scrollBox->ClipsChildren());
}

TEST(ScrollBoxTest, ScrollBarStyleFallsBackToNormalAndKeepsSizingMetrics)
{
    const auto scrollBox = CreateRef<ScrollBox>();

    SScrollBarStyle style;
    style.Thickness = 12.0f;
    style.MinimumThumbLength = 20.0f;
    style.Normal.Thumb.Color = { 0.3f, 0.4f, 0.5f, 1.0f };
    scrollBox->SetStyle(style);

    EXPECT_EQ(scrollBox->GetStyle().Thickness, 12.0f);
    EXPECT_EQ(scrollBox->GetStyle().MinimumThumbLength, 20.0f);
    EXPECT_EQ(scrollBox->GetStyle().Get(EStyleLayer::Hovered).Thumb.Color,
              SColor(0.3f, 0.4f, 0.5f, 1.0f));
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
