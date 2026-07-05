#include <gtest/gtest.h>
using namespace testing;

#include <Engine/GUI/VerticalBox.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // Minimal concrete leaf widget (no font/GPU dependencies).
    class LeafWidget final : public Widget
    {
      public:
        glm::vec2 ComputeDesiredSize() override { return {}; }
        void BuildDrawCommands(RenderBatch& batch, int zOrder) override {}
    };

    // Minimal concrete single-child widget.
    class ContentTestWidget final : public ContentWidget
    {
      public:
        glm::vec2 ComputeDesiredSize() override { return {}; }
        void BuildDrawCommands(RenderBatch& batch, int zOrder) override {}
    };
}

TEST(VisitChildrenTest, LeafWidgetHasNoChildren)
{
    const auto widget = CreateRef<LeafWidget>();

    int count = 0;
    widget->VisitChildren([&](const Ref<Widget>&) { ++count; });
    EXPECT_EQ(count, 0);
}

TEST(VisitChildrenTest, PanelVisitsEveryChildInOrder)
{
    const auto box = CreateRef<VerticalBox>();
    const auto a = CreateRef<LeafWidget>();
    const auto b = CreateRef<LeafWidget>();
    box->AddChild(a);
    box->AddChild(b);

    std::vector<Ref<Widget>> seen;
    box->VisitChildren([&](const Ref<Widget>& child) { seen.push_back(child); });

    ASSERT_EQ(seen.size(), 2u);
    EXPECT_EQ(seen[0], a);
    EXPECT_EQ(seen[1], b);
}

TEST(VisitChildrenTest, ContentWidgetVisitsContentOnlyWhenPresent)
{
    const auto content = CreateRef<ContentTestWidget>();

    int count = 0;
    content->VisitChildren([&](const Ref<Widget>&) { ++count; });
    EXPECT_EQ(count, 0) << "no content set yet";

    const auto child = CreateRef<LeafWidget>();
    content->SetContent(child);

    Ref<Widget> seen;
    content->VisitChildren([&](const Ref<Widget>& c) { seen = c; });
    EXPECT_EQ(seen, child);
}
