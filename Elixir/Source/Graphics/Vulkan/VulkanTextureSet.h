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

        SResourceHandle AddTexture(const Ref<Texture>& texture) override;
        void RemoveTexture(SResourceHandle handle) override;

        VkDescriptorSet GetDescriptorSet() const;
        VkDescriptorSetLayout GetDescriptorSetLayout() const;

      private:
        Ref<VulkanBindlessDescriptorPool> m_Pool;
        const VulkanGraphicsContext* m_GraphicsContext;
    };
}