#include <gtest/gtest.h>
using namespace testing;

#include <Engine/Graphics/Image.h>
using namespace Elixir;

TEST(ImageTest, Image_IsNotConstructibleAndAssignable)
{
    EXPECT_FALSE(std::is_constructible_v<Image>);
    EXPECT_FALSE(std::is_copy_constructible_v<Image>);
    EXPECT_FALSE(std::is_copy_assignable_v<Image>);
    EXPECT_FALSE(std::is_move_constructible_v<Image>);
    EXPECT_FALSE(std::is_move_assignable_v<Image>);
}

TEST(ImageTest, DepthStencilImage_IsNotConstructibleAndAssignable)
{
    EXPECT_FALSE(std::is_constructible_v<DepthStencilImage>);
    EXPECT_FALSE(std::is_copy_constructible_v<DepthStencilImage>);
    EXPECT_FALSE(std::is_copy_assignable_v<DepthStencilImage>);
    EXPECT_FALSE(std::is_move_constructible_v<DepthStencilImage>);
    EXPECT_FALSE(std::is_move_assignable_v<DepthStencilImage>);
}