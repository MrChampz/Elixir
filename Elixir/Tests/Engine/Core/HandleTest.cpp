#include <gtest/gtest.h>

#include <Engine/Core/Handle.h>
using namespace Elixir;

using TestHandle = Handle<uint32_t>;

TEST(HandleTest, DefaultConstructorIsNull)
{
    constexpr TestHandle handle;
    EXPECT_TRUE(handle.IsNull());
    EXPECT_FALSE(handle.IsValid());
}

TEST(HandleTest, ConstructorSetsValidIndex)
{
    const TestHandle handle(10);
    EXPECT_FALSE(handle.IsNull());
    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(handle.GetIndex(), 10);
}

TEST(HandleTest, NullIndexIsMaxValue)
{
    constexpr TestHandle handle;
    EXPECT_EQ(handle.GetIndexUnchecked(), std::numeric_limits<uint32_t>::max());
}

TEST(HandleTest, EqualityAndInequality)
{
    const TestHandle h1(5), h2(5), h3(7);
    EXPECT_TRUE(h1 == h2);
    EXPECT_FALSE(h1 != h2);
    EXPECT_TRUE(h1 != h3);
}

TEST(HandleTest, RelationalOperators)
{
    const TestHandle h1(5), h2(7);
    EXPECT_TRUE(h1 < h2);
    EXPECT_TRUE(h2 > h1);
    EXPECT_TRUE(h1 <= h2);
    EXPECT_TRUE(h2 >= h1);
    EXPECT_TRUE(h1 <= h1);
    EXPECT_TRUE(h2 >= h2);
}

TEST(HandleTest, IncrementOperators)
{
    TestHandle handle(5);
    ++handle;
    EXPECT_EQ(handle.GetIndex(), 6u);
    --handle;
    EXPECT_EQ(handle.GetIndex(), 5u);
}

TEST(HandleTest, PlusEquals)
{
    TestHandle handle(5);
    handle += 3;
    EXPECT_EQ(handle.GetIndex(), 8u);
}

TEST(HandleTest, MinusEquals)
{
    TestHandle handle(5);
    handle -= 2;
    EXPECT_EQ(handle.GetIndex(), 3u);
}

TEST(HandleTest, PlusOperator)
{
    TestHandle handle(5);
    handle = handle + 4;
    EXPECT_EQ(handle.GetIndex(), 9u);
}

TEST(HandleTest, MinusOperator)
{
    TestHandle handle(5);
    handle = handle - 3;
    EXPECT_EQ(handle.GetIndex(), 2u);
}

TEST(HandleTest, MinReturnsSmallerIndex)
{
    const TestHandle h1(3), h2(7);
    EXPECT_EQ(TestHandle::Min(h1, h2).GetIndex(), 3u);
}

TEST(HandleTest, MaxReturnsLargerIndex)
{
    const TestHandle h1(3), h2(7);
    EXPECT_EQ(TestHandle::Max(h1, h2).GetIndex(), 7u);
}

TEST(HandleTest, GetIndexOnNullShouldFail)
{
    constexpr TestHandle handle;
    EXPECT_DEATH(handle.GetIndex(), "");
}

TEST(HandleTest, ComparisionWithNullShouldFail)
{
    const TestHandle h1, h2(5);
    EXPECT_DEATH(h1 < h2, "");
    EXPECT_DEATH(h2 < h1, "");
}