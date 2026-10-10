#pragma once

#include <Engine/Graphics/TextureSet.h>
#include <Graphics/Vulkan/VulkanDescriptorPool.h>

namespace Elixir::Vulkan
{
    class ELIXIR_API VulkanTextureSet final : public TextureSet
    {
      public:
        explicit VulkanTextureSet(const GraphicsContext* context);
        ~VulkanTextureSet() override;

        void Clear() override;

        /** @brief Registers a sampled image in the shared descriptor pool. */
        SResourceHandle AddImage(const Ref<Image>& image) override;

        /** @brief Releases this set's reference to a registered image. */
        void RemoveImage(SResourceHandle handle) override;

        VkDescriptorSet GetDescriptorSet() const;
        VkDescriptorSetLayout GetDescriptorSetLayout() const;

      private:
        Ref<VulkanBindlessDescriptorPool> m_Pool;
        const VulkanGraphicsContext* m_GraphicsContext;
    };
}
