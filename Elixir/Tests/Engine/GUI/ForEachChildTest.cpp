#include <gtest/gtest.h>
using namespace testing;

#include <Engine/GUI/VerticalBox.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // Minimal concrete leaf widget (no font/GPU dependencies).
    // The using-declaration promotes the protected ForEachChild to public for the test.
    class LeafWidget final : public Widget
    {
      public:
        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return {}; }
        using Widget::ForEachChild;
    };

    // Minimal concrete single-child widget exercising ContentWidget::ForEachChild.
    class ContentTestWidget final : public ContentWidget
    {
      public:
        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return {}; }
        using ContentWidget::ForEachChild;
    };

    // Minimal multi-child container exercising Panel::ForEachChild.
    // VerticalBox is final, so we drive TPanel<LayoutSlot> directly instead - the same base
    // VerticalBox itself now uses. It already provides AddChild (see Panel.h), so this no
    // longer needs to hand-roll the push_back/AttachChild pair Panel::m_Slots used to allow.
    class PanelTestWidget final : public TPanel<LayoutSlot>
    {
      public:
        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return {}; }

        using Panel::ForEachChild;
    };
}

TEST(ForEachChildTest, LeafWidgetHasNoChildren)
{
    const auto widget = CreateRef<LeafWidget>();

    int count = 0;
    widget->ForEachChild([&](const Ref<Widget>&) { ++count; });
    EXPECT_EQ(count, 0);
}

TEST(ForEachChildTest, PanelVisitsEveryChildInOrder)
{
    const auto box = CreateRef<PanelTestWidget>();
    const auto a = CreateRef<LeafWidget>();
    const auto b = CreateRef<LeafWidget>();
    box->AddChild(a);
    box->AddChild(b);

    std::vector<Ref<Widget>> seen;
    box->ForEachChild([&](const Ref<Widget>& child) { seen.push_back(child); });

    ASSERT_EQ(seen.size(), 2u);
    EXPECT_EQ(seen[0], a);
    EXPECT_EQ(seen[1], b);
}

TEST(ForEachChildTest, ContentWidgetHasNoChildrenWhenEmpty)
{
    const auto content = CreateRef<ContentTestWidget>();

    int count = 0;
    content->ForEachChild([&](const Ref<Widget>&) { ++count; });
    EXPECT_EQ(count, 0) << "no content set yet";
}

TEST(ForEachChildTest, ContentWidgetVisitsContentExactlyOnce)
{
    const auto content = CreateRef<ContentTestWidget>();
    const auto child = CreateRef<LeafWidget>();
    content->SetContent(child);

    std::vector<Ref<Widget>> seen;
    content->ForEachChild([&](const Ref<Widget>& c) { seen.push_back(c); });

    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0], child);
}

TEST(ForEachChildTest, ContentWidgetStopsVisitingAfterClear)
{
    const auto content = CreateRef<ContentTestWidget>();
    content->SetContent(CreateRef<LeafWidget>());
    content->ClearContent();

    int count = 0;
    content->ForEachChild([&](const Ref<Widget>&) { ++count; });
    EXPECT_EQ(count, 0);
}
