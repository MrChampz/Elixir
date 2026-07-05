#include <gtest/gtest.h>
using namespace testing;

#include <Engine/GUI/VerticalBox.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // Minimal leaf widget that records how many times it actually re-arranged
    // (i.e. how often ArrangeChildren did work instead of short-circuiting).
    class CountingWidget final : public Widget
    {
      public:
        int ArrangeCount = 0;

        explicit CountingWidget(const glm::vec2& desired = { 10.0f, 10.0f })
        {
            m_DesiredSize = desired;
        }

        glm::vec2 ComputeDesiredSize() override { return m_DesiredSize; }

        void GenerateDrawCommands(RenderBatch& batch, int zOrder) override {}

        void ArrangeChildren(const SRect& allocatedSpace) override
        {
            if (!m_LayoutDirty && m_LastArrangedSpace == allocatedSpace)
                return;

            ++ArrangeCount;
            m_Geometry = allocatedSpace;
            m_LastArrangedSpace = allocatedSpace;
            m_LayoutDirty = false;
        }
    };

    // Arrange via a Widget reference so the public base overload is used
    // (containers re-declare ArrangeChildren as protected).
    void Arrange(const Ref<Widget>& widget, const SRect& space)
    {
        widget->ArrangeChildren(space);
    }
}

TEST(DirtyTrackingTest, WidgetStartsDirtyAndIsCleanedByArrange)
{
    const auto widget = CreateRef<CountingWidget>();
    EXPECT_TRUE(widget->IsLayoutDirty());

    Arrange(widget, { { 0, 0 }, { 50, 50 } });
    EXPECT_FALSE(widget->IsLayoutDirty());
    EXPECT_EQ(widget->ArrangeCount, 1);
}

TEST(DirtyTrackingTest, CleanWidgetSkipsArrangeWithSameSpaceButNotWithNewSpace)
{
    const auto widget = CreateRef<CountingWidget>();

    Arrange(widget, { { 0, 0 }, { 50, 50 } });
    EXPECT_EQ(widget->ArrangeCount, 1);

    // Same space, still clean -> skipped.
    Arrange(widget, { { 0, 0 }, { 50, 50 } });
    EXPECT_EQ(widget->ArrangeCount, 1);

    // Different space -> must re-arrange even though not self-dirty.
    Arrange(widget, { { 0, 0 }, { 60, 60 } });
    EXPECT_EQ(widget->ArrangeCount, 2);
}

TEST(DirtyTrackingTest, MarkDirtyPropagatesToAncestors)
{
    const auto root = CreateRef<VerticalBox>();
    const auto mid = CreateRef<VerticalBox>();
    const auto leaf = CreateRef<CountingWidget>();

    root->AddChild(mid);
    mid->AddChild(leaf);

    Arrange(root, { { 0, 0 }, { 100, 100 } });
    EXPECT_FALSE(root->IsLayoutDirty());
    EXPECT_FALSE(mid->IsLayoutDirty());
    EXPECT_FALSE(leaf->IsLayoutDirty());

    // Dirtying a deep leaf must mark its ancestors up to the root.
    leaf->MarkLayoutDirty();
    EXPECT_TRUE(leaf->IsLayoutDirty());
    EXPECT_TRUE(mid->IsLayoutDirty());
    EXPECT_TRUE(root->IsLayoutDirty());
}

TEST(DirtyTrackingTest, LayoutSetterInvalidatesButVisualSetterDoesNot)
{
    const auto root = CreateRef<VerticalBox>();
    const auto child = CreateRef<VerticalBox>();
    root->AddChild(child);

    Arrange(root, { { 0, 0 }, { 100, 100 } });
    ASSERT_FALSE(root->IsLayoutDirty());

    // Padding affects layout -> invalidates the child and propagates to the root.
    child->SetPadding(SPadding(5.0f));
    EXPECT_TRUE(child->IsLayoutDirty());
    EXPECT_TRUE(root->IsLayoutDirty());

    Arrange(root, { { 0, 0 }, { 100, 100 } });
    ASSERT_FALSE(child->IsLayoutDirty());

    // Background colour is purely visual (redrawn every frame) -> no relayout.
    child->SetBackground(SColor(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_FALSE(child->IsLayoutDirty());
    EXPECT_FALSE(root->IsLayoutDirty());
}

TEST(DirtyTrackingTest, CleanSiblingsAreNotReArranged)
{
    const auto root = CreateRef<VerticalBox>();
    const auto a = CreateRef<CountingWidget>(glm::vec2{ 10.0f, 10.0f });
    const auto b = CreateRef<CountingWidget>(glm::vec2{ 10.0f, 10.0f });
    root->AddChild(a);
    root->AddChild(b);

    const SRect space{ { 0, 0 }, { 100, 100 } };

    Arrange(root, space);
    EXPECT_EQ(a->ArrangeCount, 1);
    EXPECT_EQ(b->ArrangeCount, 1);

    // Only 'a' changes; 'b' keeps the same allocated space and must be skipped.
    a->MarkLayoutDirty();
    Arrange(root, space);
    EXPECT_EQ(a->ArrangeCount, 2);
    EXPECT_EQ(b->ArrangeCount, 1);
}

TEST(DirtyTrackingTest, AllFlagsClearedAfterArrangePass)
{
    const auto root = CreateRef<VerticalBox>();
    const auto a = CreateRef<CountingWidget>();
    const auto b = CreateRef<CountingWidget>();
    root->AddChild(a);
    root->AddChild(b);

    Arrange(root, { { 0, 0 }, { 100, 100 } });

    EXPECT_FALSE(root->IsLayoutDirty());
    EXPECT_FALSE(a->IsLayoutDirty());
    EXPECT_FALSE(b->IsLayoutDirty());
}
