#include <gtest/gtest.h>

#include <Engine/Graphics/Image.h>
#include <Engine/Graphics/Texture.h>

using namespace Elixir;

namespace
{
    class ImageMetadata final : public Image
    {
      public:
        explicit ImageMetadata(const SImageCreateInfo& info) : Image(nullptr, info) {}
        void Destroy() override {}
        void Transition(const CommandBuffer*, EImageLayout) override {}
        void Barrier(const CommandBuffer*) override {}
        void CopyFrom(const CommandBuffer*, const Buffer*, std::span<SBufferImageCopy>) override {}
        bool IsValid() const override { return false; }

      private:
        void CreateResource(const SImageCreateInfo&) override {}
        void GenerateMipmaps(const CommandBuffer*, EImageLayout) override {}
        void CopyMip(const CommandBuffer*, Image*, const Extent3D&, const Extent3D&, uint32_t) override {}
    };
}

TEST(ImageTest, ImageCannotBeCopiedOrMoved)
{
    EXPECT_TRUE(std::is_abstract_v<Image>);
    EXPECT_FALSE(std::is_copy_constructible_v<Image>);
    EXPECT_FALSE(std::is_copy_assignable_v<Image>);
    EXPECT_FALSE(std::is_move_constructible_v<Image>);
    EXPECT_FALSE(std::is_move_assignable_v<Image>);
}

TEST(ImageTest, TextureAssetsUseComposition)
{
    EXPECT_FALSE((std::is_base_of_v<Image, Texture>));
    EXPECT_FALSE((std::is_base_of_v<Texture2D, Texture3D>));
    EXPECT_TRUE((std::is_base_of_v<Texture, Texture2D>));
    EXPECT_TRUE((std::is_base_of_v<Texture, Texture3D>));
    EXPECT_FALSE(std::is_copy_constructible_v<Texture2D>);
}

TEST(ImageTest, TextureMappingDoesNotResolveMipmapsOrUploadUsage)
{
    STexture2DCreateInfo textureInfo;
    textureInfo.Format = EImageFormat::R8G8B8A8_UNORM;
    textureInfo.Width = 8;
    textureInfo.Height = 4;
    textureInfo.MipLevels = 99;
    textureInfo.MipmapMode = EImageMipmapMode::NormalMap;
    const auto imageInfo = Texture2D::CreateImageInfo(textureInfo);
    EXPECT_EQ(imageInfo.MipLevels, 99u);
    EXPECT_EQ(imageInfo.MipmapMode, EImageMipmapMode::NormalMap);
    EXPECT_EQ(imageInfo.Usage, EImageUsage::Sampled);
    EXPECT_EQ(Image::GetMipLevelCount(imageInfo), 4u);
}

TEST(ImageTest, TextureCreationPropagatesImageCreationFailure)
{
    STextureCreateInfo textureInfo;
    textureInfo.Width = 1;
    EXPECT_FALSE(Texture::Create(nullptr, textureInfo));

    STexture2DCreateInfo texture2DInfo;
    texture2DInfo.Width = 1;
    texture2DInfo.Height = 1;
    EXPECT_FALSE(Texture2D::Create(nullptr, texture2DInfo));

    STexture3DCreateInfo texture3DInfo;
    texture3DInfo.Width = 1;
    texture3DInfo.Height = 1;
    texture3DInfo.Depth = 1;
    EXPECT_FALSE(Texture3D::Create(nullptr, texture3DInfo));
}

TEST(ImageTest, MipCountUsesTheLargestDimension)
{
    EXPECT_EQ(Image::GetFullMipLevelCount({ 8, 4, 1 }), 4u);
    EXPECT_EQ(Image::GetFullMipLevelCount({ 1, 1, 1 }), 1u);
    EXPECT_EQ(Image::GetFullMipLevelCount({ 2, 8, 16 }), 5u);
    EXPECT_EQ(Image::GetFullMipLevelCount({ 7, 3, 1 }), 3u);
}

TEST(ImageTest, GeneratedModesIgnoreExplicitMipCount)
{
    SImageCreateInfo info{ .Width = 8, .Height = 4 };
    for (const auto mode : { EImageMipmapMode::SimpleAverage, EImageMipmapMode::NormalMap })
    {
        info.MipmapMode = mode;
        info.MipLevels = 0;
        EXPECT_EQ(Image::GetMipLevelCount(info), 4u);
        info.MipLevels = 99;
        EXPECT_EQ(Image::GetMipLevelCount(info), 4u);
    }
}

TEST(ImageTest, NoMipmapsAlwaysResolvesOneLevel)
{
    SImageCreateInfo info{ .Width = 8, .Height = 4, .MipLevels = 99 };
    EXPECT_EQ(Image::GetMipLevelCount(info), 1u);
}

TEST(ImageTest, LeaveExistingMipsPreservesTheCount)
{
    SImageCreateInfo info{
        .Width = 8, .Height = 4,
        .MipmapMode = EImageMipmapMode::LeaveExistingMips, .MipLevels = 3
    };
    EXPECT_EQ(Image::GetMipLevelCount(info), 3u);
}

TEST(ImageTest, ImageUsageCanTestAnyFlags)
{
    const auto usage = EImageUsage::Sampled | EImageUsage::TransferDst;
    EXPECT_TRUE(HasAnyFlags(usage, EImageUsage::Sampled | EImageUsage::Storage));
    EXPECT_FALSE(HasAnyFlags(usage, EImageUsage::Storage | EImageUsage::ColorAttachment));
}

TEST(ImageTest, AllocationDescriptionDoesNotKeepUploadDataOrGenerationRequests)
{
    std::array<uint8_t, 8 * 4 * 4> pixels{};
    SImageCreateInfo info{
        .InitialData = pixels.data(),
        .Width = 8, .Height = 4,
        .Format = EImageFormat::R8G8B8A8_UNORM,
        .MipmapMode = EImageMipmapMode::NormalMap,
        .MipLevels = 4,
        .AllocationInfo = { .RequiredFlags = EMemoryProperty::HostVisible }
    };
    ImageMetadata image(info);
    const auto allocation = image.GetCreateInfo();
    EXPECT_EQ(allocation.InitialData, nullptr);
    EXPECT_TRUE(allocation.InitialMipData.empty());
    EXPECT_EQ(allocation.MipmapMode, EImageMipmapMode::LeaveExistingMips);
    EXPECT_EQ(allocation.MipLevels, 4u);
    EXPECT_EQ(allocation.AllocationInfo.RequiredFlags, EMemoryProperty::HostVisible);
}

TEST(ImageTest, BaseSizeIncludesArrayLayersAndCompressedEdgeBlocks)
{
    ImageMetadata image({
        .Width = 5, .Height = 3,
        .Format = EImageFormat::BC1_RGB_UNORM_BLOCK,
        .MipLevels = 3, .ArrayLayers = 2,
    });
    EXPECT_EQ(image.GetMipSize(0), 16u);
    EXPECT_EQ(image.GetMipSize(1), 8u);
    EXPECT_EQ(image.GetMipSize(2), 8u);
    EXPECT_EQ(image.GetSize(), 32u);
}

TEST(ImageTest, Texture3DMapsAllDimensionsWithoutResolvingMipCount)
{
    STexture3DCreateInfo info;
    info.Width = 4;
    info.Height = 2;
    info.Depth = 8;
    info.MipmapMode = EImageMipmapMode::SimpleAverage;
    const auto image = Texture3D::CreateImageInfo(info);
    EXPECT_EQ(image.Type, EImageType::_3D);
    EXPECT_EQ(image.Width, 4u);
    EXPECT_EQ(image.Height, 2u);
    EXPECT_EQ(image.Depth, 8u);
    EXPECT_EQ(image.MipLevels, 1u);
    EXPECT_EQ(Image::GetMipLevelCount(image), 4u);
}
