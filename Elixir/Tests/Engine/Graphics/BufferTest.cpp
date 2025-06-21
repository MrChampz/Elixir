#include <gtest/gtest.h>
using namespace testing;

#include <Engine/Graphics/Buffer.h>
using namespace Elixir;

TEST(BufferTest, Buffer_IsNotConstructibleAndAssignable)
{
    EXPECT_FALSE(std::is_constructible_v<Buffer>);
    EXPECT_FALSE(std::is_copy_constructible_v<Buffer>);
    EXPECT_FALSE(std::is_copy_assignable_v<Buffer>);
    EXPECT_FALSE(std::is_move_constructible_v<Buffer>);
    EXPECT_FALSE(std::is_move_assignable_v<Buffer>);
}

TEST(BufferTest, DynamicBuffer_IsNotConstructibleAndAssignable)
{
    EXPECT_FALSE(std::is_constructible_v<DynamicBuffer>);
    EXPECT_FALSE(std::is_copy_constructible_v<DynamicBuffer>);
    EXPECT_FALSE(std::is_copy_assignable_v<DynamicBuffer>);
    EXPECT_FALSE(std::is_move_constructible_v<DynamicBuffer>);
    EXPECT_FALSE(std::is_move_assignable_v<DynamicBuffer>);
}

TEST(BufferTest, StagingBuffer_IsNotConstructibleAndAssignable)
{
    EXPECT_FALSE(std::is_constructible_v<StagingBuffer>);
    EXPECT_FALSE(std::is_copy_constructible_v<StagingBuffer>);
    EXPECT_FALSE(std::is_copy_assignable_v<StagingBuffer>);
    EXPECT_FALSE(std::is_move_constructible_v<StagingBuffer>);
    EXPECT_FALSE(std::is_move_assignable_v<StagingBuffer>);
}

TEST(BufferTest, VertexBuffer_IsNotConstructibleAndAssignable)
{
    EXPECT_FALSE(std::is_constructible_v<VertexBuffer>);
    EXPECT_FALSE(std::is_copy_constructible_v<VertexBuffer>);
    EXPECT_FALSE(std::is_copy_assignable_v<VertexBuffer>);
    EXPECT_FALSE(std::is_move_constructible_v<VertexBuffer>);
    EXPECT_FALSE(std::is_move_assignable_v<VertexBuffer>);
}

TEST(BufferTest, IndexBuffer_IsNotConstructibleAndAssignable)
{
    EXPECT_FALSE(std::is_constructible_v<IndexBuffer>);
    EXPECT_FALSE(std::is_copy_constructible_v<IndexBuffer>);
    EXPECT_FALSE(std::is_copy_assignable_v<IndexBuffer>);
    EXPECT_FALSE(std::is_move_constructible_v<IndexBuffer>);
    EXPECT_FALSE(std::is_move_assignable_v<IndexBuffer>);
}