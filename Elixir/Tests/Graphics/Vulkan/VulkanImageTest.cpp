#include <gtest/gtest.h>

#include <Engine/Graphics/Texture.h>
#include <Graphics/Vulkan/VulkanImage.h>
#include <Graphics/Vulkan/VulkanBuffer.h>
#include <Graphics/Vulkan/VulkanCommandBuffer.h>
#include "VulkanTestContext.h"

#include <array>

using namespace Elixir;
using namespace Elixir::Vulkan;

class VulkanImageTest : public testing::Test
{
  protected:
    static void SetUpTestSuite()
    {
        Context = VulkanTestContext::Get().GetGraphicsContext();
    }

    static std::vector<uint8_t> ReadMip(const Ref<Image>& image, uint32_t level, uint32_t layer = 0)
    {
        SBufferCreateInfo bufferInfo{};
        bufferInfo.Buffer = SBuffer(image->GetMipSize(level));
        bufferInfo.Usage = EBufferUsage::TransferDst;
        bufferInfo.AllocationInfo.RequiredFlags = EMemoryProperty::HostVisible | EMemoryProperty::HostCoherent;
        const Ref<StagingBuffer> buffer = CreateRef<VulkanStagingBuffer>(Context, bufferInfo);
        auto cmd = Context->GetUploadCommandBuffer();
        const auto layout = image->GetLayout();
        cmd->Begin();
        image->Transition(cmd, EImageLayout::TransferSrc);
        VkBufferImageCopy region{};
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level, layer, 1 };
        const auto extent = image->GetMipExtent(level);
        region.imageExtent = { extent.Width, extent.Height, extent.Depth };
        vkCmdCopyImageToBuffer(
            static_cast<VulkanCommandBuffer*>(cmd.get())->GetVulkanCommandBuffer(),
            TryToGetVulkanImageHandle(image.get()), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            static_cast<const VulkanStagingBuffer*>(buffer.get())->GetVulkanBuffer(), 1, &region
        );
        VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(
            static_cast<VulkanCommandBuffer*>(cmd.get())->GetVulkanCommandBuffer(),
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
            1, &barrier, 0, nullptr, 0, nullptr
        );
        image->Transition(cmd, layout);
        cmd->Flush();
        const auto* data = static_cast<const uint8_t*>(buffer->Map());
        std::vector<uint8_t> result(data, data + image->GetMipSize(level));
        buffer->Unmap();
        return result;
    }

    static GraphicsContext* Context;
};

GraphicsContext* VulkanImageTest::Context = nullptr;

TEST_F(VulkanImageTest, VulkanImageCannotBeCopiedOrMoved)
{
    EXPECT_FALSE(std::is_copy_constructible_v<VulkanImage>);
    EXPECT_FALSE(std::is_copy_assignable_v<VulkanImage>);
    EXPECT_FALSE(std::is_move_constructible_v<VulkanImage>);
    EXPECT_FALSE(std::is_move_assignable_v<VulkanImage>);
}

TEST_F(VulkanImageTest, CreatesAndDestroysAnImage)
{
    const auto image = Image::Create(Context, EImageFormat::R8G8B8A8_SRGB, 800);
    ASSERT_NE(TryToGetVulkanImageHandle(image.get()), VK_NULL_HANDLE);
    EXPECT_EQ(image->GetWidth(), 800u);
    EXPECT_EQ(image->GetSize(), 800u * 4u);
    EXPECT_EQ(image->GetType(), EImageType::_1D);
    EXPECT_EQ(image->GetFormat(), EImageFormat::R8G8B8A8_SRGB);
    EXPECT_EQ(image->GetMipLevels(), 1u);
    EXPECT_EQ(
        image->GetUsage(),
        EImageUsage::Sampled | EImageUsage::TransferSrc | EImageUsage::TransferDst
    );
    EXPECT_EQ(image->GetLayout(), EImageLayout::Undefined);
    image->Destroy();
    EXPECT_FALSE(image->IsValid());
    EXPECT_NO_THROW(image->Destroy());
    EXPECT_EQ(TryToGetVulkanImageHandle(nullptr), VK_NULL_HANDLE);
}

TEST_F(VulkanImageTest, DepthImagesUseTheSameBackend)
{
    for (const auto format : { EImageFormat::D32_SFLOAT, EImageFormat::D32_SFLOAT_S8_UINT })
    {
        const bool stencil = format == EImageFormat::D32_SFLOAT_S8_UINT;
        SImageCreateInfo info{
            .Width = 64, .Height = 32, .Format = format,
            .Usage = EImageUsage::Sampled | EImageUsage::DepthStencilAttachment,
            .InitialLayout = stencil ? EImageLayout::DepthStencilAttachment : EImageLayout::DepthAttachment
        };
        const auto image = Image::Create(Context, info);
        ASSERT_NE(TryToGetVulkanImage(image.get()), nullptr);
        EXPECT_EQ(image->GetHeight(), 32u);
        EXPECT_EQ(image->GetLayout(), info.InitialLayout);
        EXPECT_EQ(image->GetAspect(), stencil ? EImageAspect::Depth | EImageAspect::Stencil : EImageAspect::Depth);
    }
}

TEST_F(VulkanImageTest, TextureOwnsAnInitializedImageAndMetadata)
{
    std::array<uint8_t, 8 * 4 * 4> pixels;
    pixels.fill(128);
    STexture2DCreateInfo info;
    info.InitialData = pixels.data();
    info.Format = EImageFormat::R8G8B8A8_UNORM;
    info.Width = 8;
    info.Height = 4;
    info.MipLevels = 99;
    info.MipmapMode = EImageMipmapMode::SimpleAverage;
    info.Path = "procedural/test";
    info.HDR = true;
    auto texture = Texture2D::Create(Context, info);
    const auto image = texture->GetImage();
    ASSERT_TRUE(texture->IsValid());
    EXPECT_EQ(texture->GetPath(), info.Path);
    EXPECT_TRUE(texture->IsHDR());
    EXPECT_NE(texture->GetUUID(), image->GetUUID());
    EXPECT_EQ(image->GetMipLevels(), 4u);
    EXPECT_EQ(image->GetLayout(), EImageLayout::ShaderReadOnly);
    EXPECT_EQ(image->GetUsage(), EImageUsage::Sampled | EImageUsage::TransferSrc | EImageUsage::TransferDst);
    EXPECT_EQ(ReadMip(image, 3), (std::vector<uint8_t>{128, 128, 128, 128}));
    texture.reset();
    EXPECT_TRUE(image->IsValid());
}

TEST_F(VulkanImageTest, NormalMapMipsAreGeneratedByImageAndUploaded)
{
    const std::array<uint8_t, 16> pixels{
        255, 128, 128, 20, 0, 127, 128, 40,
        128, 255, 128, 60, 127, 0, 255, 80
    };
    SImageCreateInfo info{
        .InitialData = pixels.data(),
        .Width = 2, .Height = 2,
        .Format = EImageFormat::R8G8B8A8_UNORM,
        .MipmapMode = EImageMipmapMode::NormalMap,
        .MipLevels = 99,
        .Usage = EImageUsage::Sampled | EImageUsage::TransferSrc,
        .InitialLayout = EImageLayout::ShaderReadOnly
    };
    const auto image = Image::Create(Context, info);
    EXPECT_EQ(image->GetMipLevels(), 2u);
    EXPECT_EQ(ReadMip(image, 0), (std::vector<uint8_t>(pixels.begin(), pixels.end())));
    EXPECT_EQ(ReadMip(image, 1), (std::vector<uint8_t>{128, 128, 255, 50}));
    const auto description = image->GetCreateInfo();
    EXPECT_EQ(description.MipmapMode, EImageMipmapMode::LeaveExistingMips);
    EXPECT_EQ(description.InitialData, nullptr);
    EXPECT_TRUE(description.InitialMipData.empty());
    EXPECT_NO_THROW(Image::Create(Context, description));
}

TEST_F(VulkanImageTest, LeaveExistingMipsUploadsEveryLevelAndLayerWithoutRegeneration)
{
    std::array<uint8_t, 16> base;
    base.fill(10);
    const std::array<uint8_t, 4> lower{20, 40, 60, 80};
    const std::array<SImageMipData, 4> mips{{
        { base.data(), base.size(), 0, 0 },
        { lower.data(), lower.size(), 1, 0 },
        { base.data(), base.size(), 0, 1 },
        { lower.data(), lower.size(), 1, 1 },
    }};
    SImageCreateInfo info{
        .Width = 2, .Height = 2,
        .Format = EImageFormat::R8G8B8A8_UNORM,
        .MipmapMode = EImageMipmapMode::LeaveExistingMips,
        .MipLevels = 2, .ArrayLayers = 2,
        .Usage = EImageUsage::Sampled | EImageUsage::TransferSrc,
        .InitialLayout = EImageLayout::ShaderReadOnly,
        .InitialMipData = mips,
    };
    const auto image = Image::Create(Context, info);
    EXPECT_EQ(ReadMip(image, 0), (std::vector<uint8_t>(base.begin(), base.end())));
    EXPECT_EQ(ReadMip(image, 1), (std::vector<uint8_t>(lower.begin(), lower.end())));
    EXPECT_EQ(ReadMip(image, 1, 1), (std::vector<uint8_t>(lower.begin(), lower.end())));
}

TEST_F(VulkanImageTest, SimpleAverageHandlesEveryArrayLayer)
{
    std::array<uint8_t, 32> pixels;
    std::fill_n(pixels.begin(), 16, 40);
    std::fill_n(pixels.begin() + 16, 16, 80);
    const auto image = Image::Create(Context, {
        .InitialData = pixels.data(),
        .Width = 2, .Height = 2,
        .Format = EImageFormat::R8G8B8A8_UNORM,
        .MipmapMode = EImageMipmapMode::SimpleAverage,
        .ArrayLayers = 2,
        .InitialLayout = EImageLayout::ShaderReadOnly,
    });
    EXPECT_EQ(ReadMip(image, 1, 0), (std::vector<uint8_t>(4, 40)));
    EXPECT_EQ(ReadMip(image, 1, 1), (std::vector<uint8_t>(4, 80)));
}

TEST_F(VulkanImageTest, NoMipmapsIgnoresRequestedCount)
{
    auto image = Image::Create(Context, {
        .Width = 8, .Height = 4,
        .Format = EImageFormat::R8G8B8A8_UNORM,
        .MipLevels = 99,
    });
    EXPECT_EQ(image->GetMipLevels(), 1u);
}

TEST_F(VulkanImageTest, TransferOnlyImagesDoNotRequireAView)
{
    const auto image = Image::Create(Context, {
        .Width = 4, .Height = 4, .Format = EImageFormat::R8G8B8A8_UNORM,
        .Usage = EImageUsage::TransferSrc | EImageUsage::TransferDst
    });
    ASSERT_TRUE(image->IsValid());
    EXPECT_EQ(TryToGetVulkanImage(image.get())->GetVulkanImageView(), VK_NULL_HANDLE);
}

TEST_F(VulkanImageTest, TransitionUpdatesTheDescriptorLayout)
{
    auto info = Image::CreateImageInfo(EImageFormat::R8G8B8A8_UNORM, 64);
    info.Usage |= EImageUsage::TransferDst;
    info.InitialLayout = EImageLayout::TransferDst;
    const auto image = Image::Create(Context, info);
    const auto cmd = Context->GetUploadCommandBuffer();
    cmd->Begin();
    image->Transition(cmd, EImageLayout::ShaderReadOnly);
    cmd->Flush();
    EXPECT_EQ(image->GetLayout(), EImageLayout::ShaderReadOnly);
    EXPECT_EQ(TryToGetVulkanImage(image.get())->GetVulkanDescriptorInfo().imageLayout,
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST_F(VulkanImageTest, ResizePreservesContentAndClampsTheAllocatedMipCount)
{
    std::array<uint8_t, 64> pixels;
    pixels.fill(80);
    const auto image = Image::Create(Context, {
        .InitialData = pixels.data(),
        .Width = 4, .Height = 4,
        .Format = EImageFormat::R8G8B8A8_UNORM,
        .MipmapMode = EImageMipmapMode::SimpleAverage,
        .InitialLayout = EImageLayout::ShaderReadOnly,
    });
    ASSERT_TRUE(Context->RunRenderTaskAndWait([image]() { image->Resize({ 2, 2, 1 }); }));
    EXPECT_EQ(image->GetWidth(), 2u);
    EXPECT_EQ(image->GetMipLevels(), 2u);
    EXPECT_EQ(image->GetSize(), 16u);
    EXPECT_EQ(image->GetLayout(), EImageLayout::ShaderReadOnly);
    EXPECT_EQ(ReadMip(image, 0), (std::vector<uint8_t>(16, 80)));
    EXPECT_EQ(ReadMip(image, 1), (std::vector<uint8_t>(4, 80)));
}

TEST_F(VulkanImageTest, CopyDoesNotChangeDestinationExtent)
{
    std::array<uint8_t, 64> pixels{};
    const auto source = Image::Create(Context, {
        .InitialData = pixels.data(), .Width = 4, .Height = 4,
        .Format = EImageFormat::R8G8B8A8_UNORM,
        .Usage = EImageUsage::Sampled | EImageUsage::TransferSrc,
        .InitialLayout = EImageLayout::TransferSrc,
    });
    const auto target = Image::Create(Context, {
        .Width = 4, .Height = 4, .Format = EImageFormat::R8G8B8A8_UNORM,
        .Usage = EImageUsage::Sampled | EImageUsage::TransferDst,
        .InitialLayout = EImageLayout::TransferDst,
    });
    auto cmd = Context->GetUploadCommandBuffer();
    cmd->Begin();
    source->Copy(cmd, target, { 4, 4, 1 }, { 2, 2, 1 });
    cmd->Flush();
    EXPECT_EQ(target->GetWidth(), 4u);
    EXPECT_EQ(target->GetHeight(), 4u);
}
