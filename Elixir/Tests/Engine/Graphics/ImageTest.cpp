#include <gtest/gtest.h>
using namespace testing;

#include <Engine/Graphics/Image.h>
#include <Engine/Graphics/Texture.h>
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

TEST(ImageTest, Texture2DCreateInfoBuildsAMipmapChain)
{
    STexture2DCreateInfo textureInfo;
    textureInfo.Format = EImageFormat::R8G8B8A8_UNORM;
    textureInfo.Width = 8;
    textureInfo.Height = 4;
    textureInfo.MipLevels = 4;
    textureInfo.GenerateMipmaps = true;

    const auto imageInfo = Texture2D::CreateImageInfo(textureInfo);

    EXPECT_TRUE(imageInfo.GenerateMipmaps);
    EXPECT_EQ(imageInfo.MipLevels, 4u);
    EXPECT_EQ(
        imageInfo.Usage,
        EImageUsage::Sampled | EImageUsage::TransferSrc | EImageUsage::TransferDst
    );
}
