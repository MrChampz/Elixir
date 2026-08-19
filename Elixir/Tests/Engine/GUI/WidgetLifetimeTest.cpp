#include <gtest/gtest.h>
using namespace testing;

#include "WidgetTestUtils.h"

#include <Engine/GUI/VerticalBox.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // RemoveChild is protected on Widget/Panel (no production code calls it from outside a
    // container's own AddChild/SetContent-style methods); promote it here so this file's
    // detach test can drive it directly, same idiom as PanelTestWidget in ForEachChildTest.cpp.
    class PanelTestWidget final : public TPanel<LayoutSlot>
    {
      public:
        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return {}; }

        using Panel::RemoveChild;
    };
}

TEST(WidgetLifetimeTest, MutatingChildAfterParentDestroyedIsSafe)
{
    const auto child = CreateRef<CountingWidget>();
    {
        const auto parent = CreateRef<VerticalBox>();
        parent->AddChild(child);
    }

    child->MarkLayoutDirty();
    EXPECT_TRUE(child->IsLayoutDirty());
}

TEST(WidgetLifetimeTest, ReplacedContentNoLongerBubblesDirty)
{
    const auto content = CreateRef<TestContentWidget>();
    const auto oldChild = CreateRef<CountingWidget>();
    const auto newChild = CreateRef<CountingWidget>();

    content->SetContent(oldChild);
    content->SetContent(newChild);

    Arrange(content, { { 0, 0 }, { 100, 100 } });
    ASSERT_FALSE(content->IsLayoutDirty());

    oldChild->MarkLayoutDirty();
    EXPECT_TRUE(oldChild->IsLayoutDirty());
    EXPECT_FALSE(content->IsLayoutDirty());
}

TEST(WidgetLifetimeTest, RemovedChildNoLongerDirtiesContainer)
{
    const auto box = CreateRef<PanelTestWidget>();
    const auto a = CreateRef<CountingWidget>();
    box->AddChild(a);

    Arrange(box, { { 0, 0 }, { 100, 100 } });
    box->RemoveChild(a);
    Arrange(box, { { 0, 0 }, { 100, 100 } });
    ASSERT_FALSE(box->IsLayoutDirty());

    a->MarkLayoutDirty();
    EXPECT_TRUE(a->IsLayoutDirty());
    EXPECT_FALSE(box->IsLayoutDirty());
}

TEST(WidgetLifetimeTest, ReparentingDetachesFromPreviousContainer)
{
    const auto boxA = CreateRef<VerticalBox>();
    const auto boxB = CreateRef<VerticalBox>();
    const auto child = CreateRef<CountingWidget>();

    boxA->AddChild(child);
    boxB->AddChild(child);

    EXPECT_EQ(boxA->GetSlotCount(), 0u);
    EXPECT_EQ(boxB->GetSlotCount(), 1u);

    Arrange(boxA, { { 0, 0 }, { 100, 100 } });
    Arrange(boxB, { { 0, 0 }, { 100, 100 } });
    child->MarkLayoutDirty();
    EXPECT_FALSE(boxA->IsLayoutDirty());
    EXPECT_TRUE(boxB->IsLayoutDirty());
}