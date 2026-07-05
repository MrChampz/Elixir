#include <gtest/gtest.h>
using namespace testing;

#include <Engine/GUI/VerticalBox.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // Leaf widget whose desired size can be driven from the test.
    class SizedLeaf final : public Widget
    {
      public:
        glm::vec2 ComputeDesiredSize() override { return m_DesiredSize; }
        void GenerateDrawCommands(RenderBatch& batch, int zOrder) override {}

        void SetDesiredSize(const glm::vec2& size)
        {
            m_DesiredSize = size;
            MarkLayoutDirty();
        }
    };

    // Arrange through a Widget reference (containers re-declare ArrangeChildren protected).
    void Arrange(const Ref<Widget>& widget, const SRect& space)
    {
        widget->ArrangeChildren(space);
    }
}

TEST(InvalidationTest, VisualSetterMarksRenderDirtyNotLayout)
{
    const auto box = CreateRef<VerticalBox>();
    Arrange(box, { { 0, 0 }, { 100, 100 } });
    box->ClearRenderDirty();
    ASSERT_FALSE(box->IsLayoutDirty());
    ASSERT_FALSE(box->IsRenderDirty());

    box->SetBackground(SColor(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_TRUE(box->IsRenderDirty());
    EXPECT_FALSE(box->IsLayoutDirty());
}

TEST(InvalidationTest, LayoutSetterDoesNotMarkRenderDirty)
{
    const auto box = CreateRef<VerticalBox>();
    Arrange(box, { { 0, 0 }, { 100, 100 } });
    box->ClearRenderDirty();

    box->SetPadding(SPadding(5.0f));
    EXPECT_TRUE(box->IsLayoutDirty());
    EXPECT_FALSE(box->IsRenderDirty());
}

TEST(InvalidationTest, RenderDirtyDoesNotPropagateToAncestors)
{
    const auto box = CreateRef<VerticalBox>();
    const auto child = CreateRef<SizedLeaf>();
    box->AddChild(child);
    Arrange(box, { { 0, 0 }, { 100, 100 } });
    box->ClearRenderDirty();
    child->ClearRenderDirty();

    child->MarkRenderDirty();
    EXPECT_TRUE(child->IsRenderDirty());
    EXPECT_FALSE(box->IsRenderDirty()) << "render dirty is self-only";
}

TEST(InvalidationTest, SlotSetterInvalidatesOwnerLayout)
{
    const auto box = CreateRef<VerticalBox>();
    const auto child = CreateRef<SizedLeaf>();
    auto& slot = box->AddChild(child);
    Arrange(box, { { 0, 0 }, { 100, 100 } });
    ASSERT_FALSE(box->IsLayoutDirty());

    // Mutating a slot at runtime must invalidate the owning container's layout.
    slot.SetMargin(SMargin(4.0f));
    EXPECT_TRUE(box->IsLayoutDirty());
}

TEST(InvalidationTest, ChangingChildSizePropagatesToOwner)
{
    const auto box = CreateRef<VerticalBox>();
    const auto child = CreateRef<SizedLeaf>();
    const auto arrange = [&] { const Ref<Widget> w = box; w->ArrangeChildren({ { 0, 0 }, { 100, 100 } }); };

    box->AddChild(child);
    arrange();
    ASSERT_FALSE(box->IsLayoutDirty());

    // A size change on the child propagates up through the parent link established by AddChild.
    child->SetDesiredSize({ 10.0f, 25.0f });
    EXPECT_TRUE(box->IsLayoutDirty());
}
