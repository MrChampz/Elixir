#include <gtest/gtest.h>
using namespace testing;

#include <Engine/GUI/Renderer/RenderBatch.h>
#include <Engine/GUI/Renderer/RenderPass.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // Adds a rect at the given z-order; the other rect parameters are irrelevant to LayerSpan.
    void AddRectAt(RenderBatch& batch, const int zOrder)
    {
        batch.AddRect(SRect{}, SColor{}, glm::vec4(0.0f), glm::vec4(0.0f), glm::vec4(0.0f), SOutline{}, zOrder);
    }
}

TEST(RenderBatchTest, LayerSpanIsZeroWhenEmpty)
{
    const RenderBatch batch;
    EXPECT_EQ(batch.LayerSpan(), 0);
}

TEST(RenderBatchTest, LayerSpanIsHighestZOrderPlusOne)
{
    RenderBatch batch;
    AddRectAt(batch, 0);
    AddRectAt(batch, 2);   // gap is fine: span is driven by the max, not the count
    AddRectAt(batch, 1);

    EXPECT_EQ(batch.LayerSpan(), 3);
}

TEST(RenderBatchTest, DebugCommandsDoNotAdvanceTheLayerSpan)
{
    RenderBatch batch;
    batch.AddDebugRect(SRect{});

    EXPECT_EQ(batch.LayerSpan(), 0);

    AddRectAt(batch, 2);
    EXPECT_EQ(batch.LayerSpan(), 3);
}

TEST(RenderBatchTest, DebugCommandsSortAboveEveryNormalCommand)
{
    RenderBatch batch;
    batch.AddDebugRect(SRect{});
    AddRectAt(batch, 1'000);
    batch.Sort();

    ASSERT_EQ(batch.GetCommands().size(), 2u);
    EXPECT_EQ(batch.GetCommands().back().Type, EDrawCommandType::DebugRect);
}

TEST(RenderBatchTest, AppendingDebugCommandsDoesNotOverflowTheirZOrder)
{
    RenderBatch source;
    source.AddDebugRect(SRect{});

    RenderBatch destination;
    destination.Append(source, 10'000, { { -1.0f, -1.0f }, { -1.0f, -1.0f } });

    ASSERT_EQ(destination.GetCommands().size(), 1u);
    EXPECT_EQ(destination.GetCommands().front().ZOrder, 10'000);
    EXPECT_FALSE(destination.GetCommands().front().ScissorRect.IsValid());
    EXPECT_EQ(destination.LayerSpan(), 0);
}

TEST(RenderPassTest, GrowsBufferCapacityGeometrically)
{
    EXPECT_EQ(GrowBufferCapacity(10'000, 10'001), 20'000u);
    EXPECT_EQ(GrowBufferCapacity(10'000, 25'000), 25'000u);
}

TEST(RenderPassTest, KeepsSufficientBufferCapacity)
{
    EXPECT_EQ(GrowBufferCapacity(10'000, 8'000), 10'000u);
    EXPECT_EQ(GrowBufferCapacity(0, 1), 1u);
}

TEST(RenderBatchTest, SmallAppendsUseAmortizedCapacityGrowth)
{
    RenderBatch source;
    AddRectAt(source, 0);
    RenderBatch destination;
    size_t reallocations = 0;
    size_t previousCapacity = 0;
    constexpr size_t count = 4096;
    for (size_t i = 0; i < count; ++i)
    {
        destination.Append(source, int(i), {{-1, -1}, {-1, -1}});
        const auto capacity = destination.GetCommands().capacity();
        if (capacity != previousCapacity)
            ++reallocations;
        previousCapacity = capacity;
    }
    EXPECT_LT(reallocations, 32u);
    ASSERT_EQ(destination.GetCommands().size(), count);
    EXPECT_EQ(destination.GetCommands().back().ZOrder, int(count - 1));
    EXPECT_EQ(source.GetCommands().size(), 1u);
}

TEST(RenderBatchTest, SelfAppendCopiesOriginalCommandsWithoutModifyingThem)
{
    RenderBatch batch;
    const std::string label(96, 'x');
    batch.AddText(label, {{1, 2}, {20, 10}}, {}, 16, {1, 1, 1, 1}, 4);
    batch.Append(batch, 10, {{-1, -1}, {-1, -1}}, {3, 4}, 0.5f);
    ASSERT_EQ(batch.GetCommands().size(), 2u);
    const auto& original = batch.GetCommands()[0];
    const auto& appended = batch.GetCommands()[1];
    EXPECT_EQ(original.Text, label);
    EXPECT_EQ(original.Geometry.Position, glm::vec2(1, 2));
    EXPECT_EQ(original.ZOrder, 4);
    EXPECT_FLOAT_EQ(original.Color.A, 1);
    EXPECT_EQ(appended.Text, label);
    EXPECT_EQ(appended.Geometry.Position, glm::vec2(4, 6));
    EXPECT_EQ(appended.ZOrder, 14);
    EXPECT_FLOAT_EQ(appended.Color.A, 0.5f);
}
