#include <gtest/gtest.h>
using namespace testing;

#include <Engine/GUI/Canvas.h>
using namespace Elixir;
using namespace Elixir::GUI;

TEST(CanvasTest, DefaultDesiredSizeIs100x100)
{
    const auto canvas = CreateRef<Canvas>();
    const glm::vec2 desired = canvas->Measure({ UnconstrainedSize, UnconstrainedSize });

    EXPECT_FLOAT_EQ(desired.x, 100.0f);
    EXPECT_FLOAT_EQ(desired.y, 100.0f);
}

TEST(CanvasTest, SetSizeOverridesTheDefault)
{
    const auto canvas = CreateRef<Canvas>();
    canvas->SetSize({ 320.0f, 240.0f });

    const glm::vec2 desired = canvas->Measure({ UnconstrainedSize, UnconstrainedSize });
    EXPECT_FLOAT_EQ(desired.x, 320.0f);
    EXPECT_FLOAT_EQ(desired.y, 240.0f);
}

TEST(CanvasTest, SetSizeMarksLayoutDirty)
{
    const auto canvas = CreateRef<Canvas>();
    canvas->ArrangeChildren({ { 0, 0 }, { 100, 100 } });
    ASSERT_FALSE(canvas->IsLayoutDirty());

    canvas->SetSize({ 320.0f, 240.0f });
    EXPECT_TRUE(canvas->IsLayoutDirty());
}

TEST(CanvasTest, SettingSameSizeDoesNotInvalidate)
{
    const auto canvas = CreateRef<Canvas>();
    canvas->SetSize({ 320.0f, 240.0f });
    canvas->ArrangeChildren({ { 0, 0 }, { 100, 100 } });
    ASSERT_FALSE(canvas->IsLayoutDirty());

    canvas->SetSize({ 320.0f, 240.0f });
    EXPECT_FALSE(canvas->IsLayoutDirty());
}

TEST(CanvasTest, DesiredSizeIsClampedToAvailableSpace)
{
    const auto canvas = CreateRef<Canvas>();
    canvas->SetSize({ 320.0f, 240.0f });

    // The parent only has 100x80 to offer - Canvas must not ask for more than that, same
    // rule ScrollBox follows, even though its own configured size is bigger.
    const glm::vec2 desired = canvas->Measure({ 100.0f, 80.0f });
    EXPECT_FLOAT_EQ(desired.x, 100.0f);
    EXPECT_FLOAT_EQ(desired.y, 80.0f);
}

TEST(CanvasTest, DesiredSizeIsUnaffectedByUnconstrainedAvailableSpace)
{
    const auto canvas = CreateRef<Canvas>();
    canvas->SetSize({ 320.0f, 240.0f });

    // UnconstrainedSize on an axis means "no limit from the parent" - min() against it must
    // be a no-op, so the configured size still comes through unclamped.
    const glm::vec2 desired = canvas->Measure({ UnconstrainedSize, UnconstrainedSize });
    EXPECT_FLOAT_EQ(desired.x, 320.0f);
    EXPECT_FLOAT_EQ(desired.y, 240.0f);
}
