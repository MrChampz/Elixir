#pragma once

#include "VulkanGraphicsContext.h"

#include <Engine/Graphics/Image.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Elixir::Vulkan
{
    using namespace Elixir;

    class VulkanBaseImage : public virtual Image
    {
      public:
        ~VulkanBaseImage() override;

        void Transition(const CommandBuffer* cmd, EImageLayout layout) override;

        void Copy(
            const CommandBuffer* cmd,
            const Ref<Image>& dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        ) override;

        VkFormat GetVulkanFormat() const;
        VkExtent3D GetVulkanExtent() const;

        VkImage GetImage() const { return m_Image;}
        VkImageView GetImageView() const { return m_ImageView; }
        VkDescriptorImageInfo GetDescriptorInfo() const { return m_DescriptorInfo; }

      protected:
        VulkanBaseImage(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            void* data = nullptr
        );

        void CreateImage(void* data) override;
        void UpdateSampler() override;

        VkImage m_Image = VK_NULL_HANDLE;
        VkImageView m_ImageView = VK_NULL_HANDLE;
        VkDescriptorImageInfo m_DescriptorInfo{};
        VmaAllocation m_Allocation = VK_NULL_HANDLE;

        bool m_Destroyed = false;

        const VulkanGraphicsContext* m_GraphicsContext;
    };

    class ELIXIR_API VulkanImage final : public VulkanBaseImage
    {
      public:
        VulkanImage(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            void* data = nullptr
        );
    };

    class ELIXIR_API VulkanDepthStencilImage final
        : public DepthStencilImage, public VulkanBaseImage
    {
      public:
        VulkanDepthStencilImage(
            const GraphicsContext* context,
            EDepthStencilImageFormat format,
            uint32_t width,
            uint32_t height
        );

      protected:
        void CreateImage(void* data) override;
    };

    class ELIXIR_API VulkanStorageImage final : public StorageImage, public VulkanBaseImage
    {
    public:
        VulkanStorageImage(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            uint32_t height
        );

    protected:
        void CreateImage(void* data) override;
    };
}
