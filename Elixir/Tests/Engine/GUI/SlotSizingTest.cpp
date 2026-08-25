#include <gtest/gtest.h>
using namespace testing;

#include "WidgetTestUtils.h"

#include <Engine/GUI/VerticalBox.h>
#include <Engine/GUI/HorizontalBox.h>
using namespace Elixir;
using namespace Elixir::GUI;

// Regression coverage for a real bug: VerticalBox/HorizontalBox::ComputeDesiredSize summed
// each child's raw Measure() result regardless of its slot's SSizeParam rule, while
// LayoutChildren correctly used the slot's configured pixel size for Fixed children. A Fixed
// child whose content measures smaller than its configured size made the container (and
// anything reading its desired size, e.g. a ScrollBox wrapping it) under-report how much
// space it actually occupies - which is exactly how a real ScrollBox ended up thinking its
// content fit the viewport when it didn't, and silently refused to scroll.

TEST(SlotSizingTest, VerticalBoxDesiredSizeUsesFixedSlotSizeNotMeasuredSize)
{
    const auto box = CreateRef<VerticalBox>();

    // CountingWidget measures to 10x10 - far smaller than the 26px the slot is fixed to.
    const auto a = CreateRef<CountingWidget>(glm::vec2{ 10.0f, 10.0f });
    const auto b = CreateRef<CountingWidget>(glm::vec2{ 10.0f, 10.0f });
    box->AddChild(a).SetFixedSize(26.0f);
    box->AddChild(b).SetFixedSize(26.0f);

    const glm::vec2 desired = box->Measure({ 100.0f, UnconstrainedSize });

    // Bug: this used to be 20 (10 + 10, the measured heights) instead of 52 (26 + 26, the
    // configured Fixed sizes) - the same undercount that made a real ScrollBox never scroll.
    EXPECT_FLOAT_EQ(desired.y, 52.0f);
}

TEST(SlotSizingTest, HorizontalBoxDesiredSizeUsesFixedSlotSizeNotMeasuredSize)
{
    const auto box = CreateRef<HorizontalBox>();

    const auto a = CreateRef<CountingWidget>(glm::vec2{ 10.0f, 10.0f });
    const auto b = CreateRef<CountingWidget>(glm::vec2{ 10.0f, 10.0f });
    box->AddChild(a).SetFixedSize(26.0f);
    box->AddChild(b).SetFixedSize(26.0f);

    const glm::vec2 desired = box->Measure({ UnconstrainedSize, 100.0f });

    EXPECT_FLOAT_EQ(desired.x, 52.0f);
}

TEST(SlotSizingTest, VerticalBoxDesiredSizeStillUsesMeasuredSizeForAutoSlots)
{
    const auto box = CreateRef<VerticalBox>();

    // Auto is the default slot rule - no explicit SetAutoSize() call needed.
    const auto a = CreateRef<CountingWidget>(glm::vec2{ 15.0f, 15.0f });
    const auto b = CreateRef<CountingWidget>(glm::vec2{ 15.0f, 15.0f });
    box->AddChild(a);
    box->AddChild(b);

    const glm::vec2 desired = box->Measure({ 100.0f, UnconstrainedSize });

    EXPECT_FLOAT_EQ(desired.y, 30.0f);
}

TEST(SlotSizingTest, VerticalBoxDesiredSizeIgnoresFillSlotMeasuredSize)
{
    const auto box = CreateRef<VerticalBox>();

    // A Fill slot has no intrinsic size - it stretches into whatever LayoutChildren hands
    // it - so its measured size must not contribute to the container's own desired size.
    const auto a = CreateRef<CountingWidget>(glm::vec2{ 10.0f, 10.0f });
    const auto b = CreateRef<CountingWidget>(glm::vec2{ 500.0f, 500.0f });
    box->AddChild(a).SetAutoSize();
    box->AddChild(b).SetFillSize();

    const glm::vec2 desired = box->Measure({ 100.0f, UnconstrainedSize });

    EXPECT_FLOAT_EQ(desired.y, 10.0f);
}

TEST(SlotSizingTest, VerticalBoxDesiredSizeRespectsSlotConstraints)
{
    const auto box = CreateRef<VerticalBox>();
    const auto fixed = CreateRef<CountingWidget>(glm::vec2{ 10.0f, 10.0f });
    const auto automatic = CreateRef<CountingWidget>(glm::vec2{ 10.0f, 100.0f });
    const auto fill = CreateRef<CountingWidget>(glm::vec2{ 10.0f, 500.0f });

    box->AddChild(fixed).SetFixedSize(26.0f).SetMinSize({ 0.0f, 50.0f });
    box->AddChild(automatic).SetMaxSize({ FLT_MAX, 40.0f });
    box->AddChild(fill)
        .SetFillSize()
        .SetMinSize({ 0.0f, 20.0f })
        .SetMargin(SMargin(0.0f, 5.0f));

    const glm::vec2 desired = box->Measure({ 100.0f, UnconstrainedSize });

    EXPECT_FLOAT_EQ(desired.y, 120.0f);
}

TEST(SlotSizingTest, HorizontalBoxDesiredSizeRespectsSlotConstraints)
{
    const auto box = CreateRef<HorizontalBox>();
    const auto fixed = CreateRef<CountingWidget>(glm::vec2{ 10.0f, 10.0f });
    const auto automatic = CreateRef<CountingWidget>(glm::vec2{ 100.0f, 10.0f });
    const auto fill = CreateRef<CountingWidget>(glm::vec2{ 500.0f, 10.0f });

    box->AddChild(fixed).SetFixedSize(26.0f).SetMinSize({ 50.0f, 0.0f });
    box->AddChild(automatic).SetMaxSize({ 40.0f, FLT_MAX });
    box->AddChild(fill)
        .SetFillSize()
        .SetMinSize({ 20.0f, 0.0f })
        .SetMargin(SMargin(5.0f, 0.0f));

    const glm::vec2 desired = box->Measure({ UnconstrainedSize, 100.0f });

    EXPECT_FLOAT_EQ(desired.x, 120.0f);
}

TEST(SlotSizingTest, HorizontalBoxKeepsMeasurementsAlignedAfterCollapsedSlot)
{
    const auto box = CreateRef<HorizontalBox>();
    const auto collapsed = CreateRef<CountingWidget>(glm::vec2{ 10.0f, 10.0f });
    const auto visible = CreateRef<CountingWidget>(glm::vec2{ 25.0f, 15.0f });
    collapsed->SetVisibility(EVisibility::Collapsed);
    box->AddChild(collapsed);
    box->AddChild(visible);

    Arrange(box, { { 0.0f, 0.0f }, { 100.0f, 40.0f } });

    EXPECT_EQ(collapsed->ArrangeCount, 0);
    EXPECT_EQ(visible->GetGeometry().Position, glm::vec2(0.0f, 12.5f));
    EXPECT_EQ(visible->GetGeometry().Size, glm::vec2(25.0f, 15.0f));
}

TEST(SlotSizingTest, VerticalBoxKeepsMeasurementsAlignedAfterCollapsedSlot)
{
    const auto box = CreateRef<VerticalBox>();
    const auto collapsed = CreateRef<CountingWidget>(glm::vec2{ 10.0f, 10.0f });
    const auto visible = CreateRef<CountingWidget>(glm::vec2{ 15.0f, 25.0f });
    collapsed->SetVisibility(EVisibility::Collapsed);
    box->AddChild(collapsed);
    box->AddChild(visible);

    Arrange(box, { { 0.0f, 0.0f }, { 40.0f, 100.0f } });

    EXPECT_EQ(collapsed->ArrangeCount, 0);
    EXPECT_EQ(visible->GetGeometry().Position, glm::vec2(12.5f, 0.0f));
    EXPECT_EQ(visible->GetGeometry().Size, glm::vec2(15.0f, 25.0f));
}
