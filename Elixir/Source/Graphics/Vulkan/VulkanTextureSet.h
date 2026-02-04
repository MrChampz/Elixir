#pragma once

#include <Engine/Graphics/TextureSet.h>
#include <Graphics/Vulkan/VulkanDescriptorAllocator.h>

namespace Elixir::Vulkan
{
    class ELIXIR_API VulkanTextureSet final : public TextureSet
    {
        friend class VulkanShader;
      public:
        explicit VulkanTextureSet(
            const GraphicsContext* context,
            uint32_t maxTextures = MAX_TEXTURES
        );
        ~VulkanTextureSet() override;

        void SetTexture(uint32_t index, const Ref<Texture>& texture) override;
        void RemoveTexture(uint32_t index) override;

        VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }
        VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
        VkDescriptorSetLayoutBinding GetLayoutBinding() const;
        std::vector<VkDescriptorImageInfo> GetImageInfos() const { return m_ImageInfos; }

      protected:
        VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout);

      private:
        void CreateDescriptorSetLayout();
        void InitDescriptorAllocator();
        //void AllocateDescriptorSet();
        void InitImageInfos();

        void UpdateDescriptors();

        Ref<VulkanDescriptorAllocator> m_DescriptorAllocator;
        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

        std::vector<VkDescriptorImageInfo> m_ImageInfos;

        const VulkanGraphicsContext* m_GraphicsContext;
    };
}