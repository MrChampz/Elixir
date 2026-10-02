#pragma once

#include <Engine/Graphics/Image.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Elixir::Vulkan
{
    class VulkanGraphicsContext;
    class VulkanImage;

    /** Returns the native image handle, or null for a null image. */
    ELIXIR_API VkImage TryToGetVulkanImageHandle(const Image* image);

    /** Returns the Vulkan backend, or null when the image does not use Vulkan. */
    ELIXIR_API const VulkanImage* TryToGetVulkanImage(const Image* image);

    /** @brief Implements image storage and device operations for Vulkan. */
    class ELIXIR_API VulkanImage final : public Image
    {
      public:
        /**
         * Stores the description resolved by Image::Create without allocating resources.
         * Use Image::Create to obtain a fully initialized image.
         * @param context Vulkan graphics context that must outlive the image.
         * @param info Image description with mip levels and usage flags already resolved.
         */
        VulkanImage(const GraphicsContext* context, const SImageCreateInfo& info);

        /** @brief Releases the native image and view. */
        ~VulkanImage() override;

        /** @brief Releases storage after all GPU work that uses it has completed. */
        void Destroy() override;

        using Image::Transition;

        /** @brief Records a layout transition for every mip level and layer.
         * @param cmd Recording Vulkan command buffer that receives the transition.
         * @param layout Target layout for all mip levels and array layers.
         */
        void Transition(const CommandBuffer* cmd, EImageLayout layout) override;

        /** @brief Records a memory dependency without changing the image layout. */
        void Barrier(const CommandBuffer* cmd) override;

        using Image::CopyFrom;

        /** @brief Records a buffer upload; an empty region list copies the base level of all layers.
         * @param cmd Recording Vulkan command buffer that receives the upload.
         * @param src Vulkan buffer containing the pixel data, with TransferSrc usage.
         * @param regions Buffer-to-image copy regions; empty selects the full base level
         * of every array layer.
         */
        void CopyFrom(
            const CommandBuffer* cmd,
            const Buffer* src,
            std::span<SBufferImageCopy> regions = {}
        ) override;

        /** @brief Reports whether native storage is allocated. */
        bool IsValid() const override { return m_Image != VK_NULL_HANDLE; }

        /** @brief Returns the owned image handle. */
        VkImage GetVulkanImage() const { return m_Image; }

        /** @brief Returns the view spanning every mip level and layer. */
        VkImageView GetVulkanImageView() const { return m_ImageView; }

        /** @brief Returns the generation of the current native image allocation. */
        uint64_t GetVulkanResourceGeneration() const { return m_ResourceGeneration; }

        /** @brief Returns the current view and layout; samplers are bound separately. */
        const VkDescriptorImageInfo& GetVulkanDescriptorInfo() const { return m_DescriptorInfo; }

      private:
        // Creates native storage and its view from the resolved description.
        void CreateResource(const SImageCreateInfo& info) override;

        // Generates simple-average mip levels with linear image blits.
        void GenerateMipmaps(const CommandBuffer* cmd, EImageLayout finalLayout) override;

        // Blits one mip level across all array layers.
        void CopyMip(
            const CommandBuffer* cmd,
            Image* dst,
            const Extent3D& srcExtent,
            const Extent3D& dstExtent,
            uint32_t level
        ) override;

        VkImage m_Image = VK_NULL_HANDLE;
        VkImageView m_ImageView = VK_NULL_HANDLE;
        VkDescriptorImageInfo m_DescriptorInfo{};
        VmaAllocation m_Allocation = VK_NULL_HANDLE;
        uint64_t m_ResourceGeneration = 0;

        const VulkanGraphicsContext* m_Context;
    };
}
