#pragma once

#include <Engine/Graphics/Texture.h>
#include <Graphics/Vulkan/VulkanImage.h>

namespace Elixir::Vulkan
{
    class ELIXIR_API VulkanTexture final : public Texture, public VulkanBaseImage
    {
      public:
        VulkanTexture(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            void* data = nullptr,
            const std::string& path = ""
        );
    };

    class ELIXIR_API VulkanTexture2D final : public Texture2D, public VulkanBaseImage
    {
    public:
        VulkanTexture2D(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            void* data = nullptr,
            const std::string& path = ""
        );

        // TODO: Remove
        VulkanTexture2D(
            const GraphicsContext* context,
            EImageFormat format,
            VkImage image,
            VkImageView imageView,
            VkExtent2D extent,
            EImageUsage usage
        );

        ~VulkanTexture2D();

        // TODO: Remove
        static Ref<VulkanTexture2D> FromSwapchain(
            const GraphicsContext* context,
            EImageFormat format,
            VkImage image,
            VkImageView imageView,
            VkExtent2D extent,
            EImageUsage usage = EImageUsage::ColorAttachment
        );

      protected:
        // TODO: Remove
        bool m_IsSwapchainImage = false;
    };

    class ELIXIR_API VulkanTexture3D final : public Texture3D, public VulkanBaseImage
    {
    public:
        VulkanTexture3D(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            void* data = nullptr,
            const std::string& path = ""
        );
    };
}