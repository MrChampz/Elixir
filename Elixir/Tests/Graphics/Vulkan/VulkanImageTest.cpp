#include <gtest/gtest.h>
using namespace testing;

#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Graphics/CommandBuffer.h>
#include <Graphics/Vulkan/VulkanImage.h>
using namespace Elixir::Vulkan;

class VulkanImageTest : public Test
{
  protected:
    static void SetUpTestSuite()
    {
        Memory::s_Malloc = CreateScope<SystemMalloc>();
        Window = Window::Create();
        Context = GraphicsContext::Create(EGraphicsAPI::Vulkan, Window.get());
        Context->Init();
    }

    static void TearDownTestSuite()
    {
        Context->Shutdown();
    }

    static Scope<Window> Window;
    static Scope<GraphicsContext> Context;
};

Scope<Window> VulkanImageTest::Window = nullptr;
Scope<GraphicsContext> VulkanImageTest::Context = nullptr;

TEST_F(VulkanImageTest, VulkanImageBase_IsNotConstructibleAndAssignable)
{
    EXPECT_FALSE(std::is_constructible_v<VulkanImageBase<Image>>);
    EXPECT_FALSE(std::is_copy_constructible_v<VulkanImageBase<Image>>);
    EXPECT_FALSE(std::is_copy_assignable_v<VulkanImageBase<Image>>);
    EXPECT_FALSE(std::is_move_constructible_v<VulkanImageBase<Image>>);
    EXPECT_FALSE(std::is_move_assignable_v<VulkanImageBase<Image>>);
}

TEST_F(VulkanImageTest, VulkanImage_CopyConstructorIsDeleted)
{
    EXPECT_FALSE(std::is_copy_constructible_v<VulkanImage>);
    EXPECT_FALSE(std::is_copy_assignable_v<VulkanImage>);
}

TEST_F(VulkanImageTest, VulkanImage_MoveConstructorIsDeleted)
{
    EXPECT_FALSE(std::is_move_constructible_v<VulkanImage>);
    EXPECT_FALSE(std::is_move_assignable_v<VulkanImage>);
}

TEST_F(VulkanImageTest, VulkanImage_CreationAndDestruction)
{
    const auto image = Image::Create(Context.get(), EImageFormat::R8G8B8A8_SRGB, 800);

    const auto vk_Image = dynamic_cast<VulkanImageBase<Image>*>(image.get());
    ASSERT_TRUE(vk_Image != nullptr);
    ASSERT_NE(vk_Image->GetVulkanImage(), VK_NULL_HANDLE);

    EXPECT_EQ(image->GetWidth(), 800);
    EXPECT_EQ(image->GetSize(), 800 * 32);
    EXPECT_EQ(image->GetType(), EImageType::_1D);
    EXPECT_EQ(image->GetFormat(), EImageFormat::R8G8B8A8_SRGB);
    EXPECT_EQ(image->GetMipLevels(), 1);
    EXPECT_EQ(image->GetArrayLayers(), 1);
    EXPECT_EQ(image->GetUsage(), EImageUsage::Sampled);
    EXPECT_EQ(image->GetLayout(), EImageLayout::Undefined);
    EXPECT_EQ(image->GetAspect(), EImageAspect::Color);

    image->Destroy();
    SUCCEED();
}

TEST_F(VulkanImageTest, VulkanDepthStencilImage_DepthOnly)
{
    const auto image = DepthStencilImage::Create(
        Context.get(),
        EDepthStencilImageFormat::D32_SFLOAT,
        800, 600
    );

    const auto vk_Image = dynamic_cast<VulkanImageBase<DepthStencilImage>*>(image.get());
    ASSERT_TRUE(vk_Image != nullptr);
    ASSERT_NE(vk_Image->GetVulkanImage(), VK_NULL_HANDLE);

    EXPECT_EQ(image->GetWidth(), 800);
    EXPECT_EQ(image->GetHeight(), 600);
    EXPECT_EQ(image->GetSize(), 800 * 600 * 32);
    EXPECT_EQ(image->GetType(), EImageType::_2D);
    EXPECT_EQ(image->GetFormat(), EImageFormat::D32_SFLOAT);
    EXPECT_EQ(image->GetMipLevels(), 1);
    EXPECT_EQ(image->GetArrayLayers(), 1);
    EXPECT_EQ(image->GetUsage(), EImageUsage::Sampled | EImageUsage::DepthStencilAttachment);
    EXPECT_EQ(image->GetLayout(), EImageLayout::DepthAttachment);
    EXPECT_EQ(image->GetAspect(), EImageAspect::Depth);

    image->Destroy();
    SUCCEED();
}

TEST_F(VulkanImageTest, VulkanDepthStencilImage_DepthStencil)
{
    const auto image = DepthStencilImage::Create(
        Context.get(),
        EDepthStencilImageFormat::D32_SFLOAT_S8_UINT,
        800, 600
    );

    const auto vk_Image = dynamic_cast<VulkanImageBase<DepthStencilImage>*>(image.get());
    ASSERT_TRUE(vk_Image != nullptr);
    ASSERT_NE(vk_Image->GetVulkanImage(), VK_NULL_HANDLE);

    EXPECT_EQ(image->GetWidth(), 800);
    EXPECT_EQ(image->GetHeight(), 600);
    EXPECT_EQ(image->GetSize(), 800 * 600 * 40);
    EXPECT_EQ(image->GetType(), EImageType::_2D);
    EXPECT_EQ(image->GetFormat(), EImageFormat::D32_SFLOAT_S8_UINT);
    EXPECT_EQ(image->GetMipLevels(), 1);
    EXPECT_EQ(image->GetArrayLayers(), 1);
    EXPECT_EQ(image->GetUsage(), EImageUsage::Sampled | EImageUsage::DepthStencilAttachment);
    EXPECT_EQ(image->GetLayout(), EImageLayout::DepthStencilAttachment);
    EXPECT_EQ(image->GetAspect(), EImageAspect::Depth | EImageAspect::Stencil);

    image->Destroy();
    SUCCEED();
}

TEST_F(VulkanImageTest, VulkanImage_LayoutTransition) {
    SImageCreateInfo info = Image::CreateImageInfo(EImageFormat::R8G8B8A8_UNORM, 64);
    info.Usage = EImageUsage::Sampled | EImageUsage::TransferDst;
    info.InitialLayout = EImageLayout::TransferDst;

    // Initially the image is transitioned to layout defined in "InitialLayout"
    VulkanImage image(Context.get(), info);
    EXPECT_EQ(image.GetLayout(), EImageLayout::TransferDst);

    const auto cmd = Context->GetCommandBuffer();
    cmd->Begin();

    // Now perform a manual transition
    image.Transition(cmd, EImageLayout::ShaderReadOnly);
    cmd->Flush();

    EXPECT_EQ(image.GetLayout(), Elixir::EImageLayout::ShaderReadOnly);
}

TEST_F(VulkanImageTest, VulkanImage_ImageDestruction) {
    const auto image = Image::Create(Context.get(), EImageFormat::R8G8B8A8_SRGB, 128);
    EXPECT_FALSE(image->IsDestroyed());

    image->Destroy();
    EXPECT_TRUE(image->IsDestroyed());

    // Calling destroy again should be safe
    EXPECT_NO_THROW(image->Destroy());
    EXPECT_TRUE(image->IsDestroyed());
}

TEST_F(VulkanImageTest, GetVulkanImageHandler) {
    SImageCreateInfo info = Image::CreateImageInfo(EImageFormat::R8G8B8A8_UNORM, 32);
    VulkanImage image(Context.get(), info);

    SImageCreateInfo dsInfo = DepthStencilImage::CreateImageInfo(EDepthStencilImageFormat::D16_UNORM, 64, 64);
    VulkanDepthStencilImage dstImage(Context.get(), dsInfo);

    VkImage imgHandle = GetVulkanImageHandler(&image);
    VkImage dstImgHandle = GetVulkanImageHandler(&dstImage);

    EXPECT_EQ(imgHandle, image.GetVulkanImage());
    EXPECT_EQ(dstImgHandle, dstImage.GetVulkanImage());
    EXPECT_NE(imgHandle, VK_NULL_HANDLE);
}