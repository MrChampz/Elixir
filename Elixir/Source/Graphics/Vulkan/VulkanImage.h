#pragma once

#include <Engine/Graphics/Image.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Elixir::Vulkan
{
    using namespace Elixir;

    template <class Base>
    class VulkanImageBase : public Base
    {
      public:
        virtual void Destroy() override;

        using Image::Transition;
        virtual void Transition(const CommandBuffer* cmd, EImageLayout layout) override;

        using Image::Copy;
        virtual void Copy(
            const CommandBuffer* cmd,
            const Image* dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent
        ) override;

        using Image::CopyFrom;
        virtual void CopyFrom(
            const CommandBuffer* cmd,
            const Buffer* src,
            std::span<SBufferImageCopy> regions = {}
        ) override;

        [[nodiscard]] bool IsDestroyed() const override { return m_Destroyed; }

        [[nodiscard]] VkImage GetVulkanImage() const { return m_Image; }

        VulkanImageBase& operator=(const VulkanImageBase&) = delete;
        VulkanImageBase& operator=(VulkanImageBase&&) = delete;

      protected:
        VulkanImageBase(const GraphicsContext* context, const SImageCreateInfo& info);
        VulkanImageBase(const VulkanImageBase&) = delete;
        VulkanImageBase(VulkanImageBase&&) = delete;

        void CreateImage(const SImageCreateInfo& info);
        void InitImage(const SImageCreateInfo& info);
        void CreateImageView();
        void CreateDescriptorInfo();
        void UpdateSampler() override;

      private:
        VkImage m_Image = VK_NULL_HANDLE;
        VkImageView m_ImageView = VK_NULL_HANDLE;
        VkDescriptorImageInfo m_DescriptorInfo{};

        VmaAllocation m_Allocation = VK_NULL_HANDLE;

        bool m_Destroyed = false;

        const VulkanGraphicsContext* m_GraphicsContext = nullptr;
    };

    class ELIXIR_API VulkanImage final : public VulkanImageBase<Image>
    {
      public:
        VulkanImage(
            const GraphicsContext* context,
            EImageFormat format,
            uint32_t width,
            void* data = nullptr
        );
        VulkanImage(const GraphicsContext* context, const SImageCreateInfo& info);
        ~VulkanImage() override;
    };

    class ELIXIR_API VulkanDepthStencilImage final : public VulkanImageBase<DepthStencilImage>
    {
      public:
        VulkanDepthStencilImage(
            const GraphicsContext* context,
            EDepthStencilImageFormat format,
            uint32_t width,
            uint32_t height
        );
        VulkanDepthStencilImage(const GraphicsContext* context, const SImageCreateInfo& info);
        ~VulkanDepthStencilImage() override;
    };

    inline VkImage GetVulkanImageHandler(const Image* image)
    {
        if (auto* img = dynamic_cast<const VulkanImageBase<DepthStencilImage>*>(image))
            return img->GetVulkanImage();
        if (auto* img = dynamic_cast<const VulkanImageBase<Image>*>(image))
            return img->GetVulkanImage();

        EE_CORE_ASSERT(false, "Unsupported image type!")
        return VK_NULL_HANDLE;
    }
}
