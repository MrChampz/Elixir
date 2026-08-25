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
