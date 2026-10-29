#pragma once

#include <Engine/Graphics/ImageSet.h>
#include <Graphics/Vulkan/VulkanDescriptorPool.h>

namespace Elixir::Vulkan
{
    /** @brief Registers sampled images in the context's Vulkan descriptor pool. */
    class ELIXIR_API VulkanImageSet final : public ImageSet
    {
      public:
        /** @brief Creates a set using the context's shared descriptor pool. */
        explicit VulkanImageSet(const GraphicsContext* context);

        /** @brief Releases all image registrations owned by this set. */
        ~VulkanImageSet() override;

        /** @brief Releases all image registrations owned by this set. */
        void Clear() override;

        /** @brief Registers a sampled image in the shared descriptor pool. */
        SResourceHandle AddImage(const Ref<Image>& image) override;

        /** @brief Releases this set's reference to a registered image. */
        void RemoveImage(SResourceHandle handle) override;

        /** @brief Returns the shared sampled-image descriptor array. */
        VkDescriptorSet GetDescriptorSet() const;

        /** @brief Returns the layout of the shared descriptor array. */
        VkDescriptorSetLayout GetDescriptorSetLayout() const;

      private:
        Ref<VulkanBindlessDescriptorPool> m_Pool;
        const VulkanGraphicsContext* m_GraphicsContext;
    };
}
