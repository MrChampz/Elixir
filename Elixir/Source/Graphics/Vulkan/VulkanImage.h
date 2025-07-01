#pragma once

#include <Engine/Graphics/Image.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Elixir::Vulkan
{
    using namespace Elixir;

    VkImage TryToGetVulkanImage(const Image* image);

    template <class Base>
    class VulkanBaseImage : public Base
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

        [[nodiscard]] bool IsValid() const override { return m_Image != VK_NULL_HANDLE; }

        [[nodiscard]] VkImage GetVulkanImage() const { return m_Image; }
        [[nodiscard]] const VkDescriptorImageInfo& GetVulkanDescriptorInfo() const { return m_DescriptorInfo; }

        VulkanBaseImage& operator=(const VulkanBaseImage&) = delete;
        VulkanBaseImage& operator=(VulkanBaseImage&&) = delete;

      protected:
        VulkanBaseImage(const GraphicsContext* context, const SImageCreateInfo& info);
        VulkanBaseImage(const VulkanBaseImage&) = delete;
        VulkanBaseImage(VulkanBaseImage&&) = delete;

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

        const VulkanGraphicsContext* m_GraphicsContext = nullptr;
    };

    class ELIXIR_API VulkanImage final : public VulkanBaseImage<Image>
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

    class ELIXIR_API VulkanDepthStencilImage final : public VulkanBaseImage<DepthStencilImage>
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
}
