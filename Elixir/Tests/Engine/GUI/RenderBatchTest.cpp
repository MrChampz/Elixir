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
