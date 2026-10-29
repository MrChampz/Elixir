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
    EXPECT_FALSE(destination.GetCommands().front().ClipRect.IsValid());
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

TEST(RenderBatchTest, RoundedClipKeepsItsBoundsWhenAncestorClippingCutsTheQuad)
{
    RenderBatch source;
    source.AddRect({{10, 20}, {100, 80}}, {1, 1, 1, 1}, {}, {}, {}, {}, 0);
    source.Clip({{10, 20}, {100, 80}}, {0, 0, 8, 8});
    RenderBatch destination;
    destination.Append(source, 0, {{40, 30}, {40, 50}}, {5, 6});
    const auto& command = destination.GetCommands().front();
    EXPECT_EQ(command.RoundedClipRect.Position, glm::vec2(15, 26));
    EXPECT_EQ(command.RoundedClipRect.Size, glm::vec2(100, 80));
    EXPECT_EQ(command.ClipCornerRadius, glm::vec4(0, 0, 8, 8));
    EXPECT_EQ(command.ClipRect.Position, glm::vec2(40, 30));
    EXPECT_EQ(command.ClipRect.Size, glm::vec2(40, 50));
    EXPECT_EQ(command.Border, glm::vec4(0));
    EXPECT_EQ(command.TextureMapping, ETextureMapping::Stretch);
}

TEST(RenderBatchTest, UniformClipKeepsOriginalCornersAfterAncestorClipping)
{
    RenderBatch source;
    AddRectAt(source, 0);
    source.Clip({{10, 20}, {100, 80}}, 8.0f);
    RenderBatch destination;
    destination.Append(source, 0, {{40, 30}, {40, 50}}, {5, 6});
    const auto& command = destination.GetCommands().front();
    EXPECT_EQ(command.RoundedClipRect.Position, glm::vec2(15, 26));
    EXPECT_EQ(command.RoundedClipRect.Size, glm::vec2(100, 80));
    EXPECT_EQ(command.ClipCornerRadius, glm::vec4(8));
    EXPECT_EQ(command.ClipRect.Position, glm::vec2(40, 30));
}

TEST(RenderBatchTest, PlainClipClearsPreviousRadiiAndPreservesExistingScissor)
{
    RenderBatch source;
    RenderBatch ancestor;
    ancestor.AddRect({{10, 20}, {100, 80}}, {1, 1, 1, 1}, {}, {}, {}, {}, 0);
    source.Append(ancestor, 0, {{40, 30}, {40, 50}});
    source.Clip({{10, 20}, {100, 80}}, 8.0f);
    source.Clip({{20, 25}, {80, 60}});
    const auto& command = source.GetCommands().front();
    EXPECT_EQ(command.RoundedClipRect.Position, glm::vec2(20, 25));
    EXPECT_EQ(command.RoundedClipRect.Size, glm::vec2(80, 60));
    EXPECT_EQ(command.ClipCornerRadius, glm::vec4(0));
    EXPECT_EQ(command.ClipRect.Position, glm::vec2(40, 30));
    EXPECT_EQ(command.ClipRect.Size, glm::vec2(40, 50));
}

TEST(RenderBatchTest, ClipAppliesToTextAndPreservesItsAncestorClip)
{
    RenderBatch source;
    source.AddText("Label", {{10, 20}, {100, 80}}, nullptr, 11, {1, 1, 1, 1});
    source.Clip({{10, 20}, {100, 80}}, {0, 0, 8, 8});
    RenderBatch destination;
    destination.Append(source, 0, {{40, 30}, {40, 50}}, {5, 6});
    const auto& command = destination.GetCommands().front();
    EXPECT_EQ(command.RoundedClipRect.Position, glm::vec2(15, 26));
    EXPECT_EQ(command.RoundedClipRect.Size, glm::vec2(100, 80));
    EXPECT_EQ(command.ClipCornerRadius, glm::vec4(0, 0, 8, 8));
    EXPECT_EQ(command.ClipRect.Position, glm::vec2(40, 30));
    EXPECT_EQ(command.ClipRect.Size, glm::vec2(40, 50));
}
