#pragma once

#include <Engine/Graphics/Shader/Shader.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

namespace Elixir::Vulkan
{
    using namespace Elixir;

    class ELIXIR_API VulkanShader final : public Shader
    {
      public:
        VulkanShader(const GraphicsContext* context, SShaderCreateInfo&& info);
        ~VulkanShader() override;

      protected:
        void CreateDescriptorSetLayouts();
        void CreateDescriptorSets();

        void UpdateDescriptorSets();

        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding,
            const Ref<Texture>& texture
        ) const;
        void UpdateDescriptorSet(SShaderBinding binding, const Ref<Texture>& texture) const;

        VkWriteDescriptorSet GetWriteDescriptorSet(
            SShaderBinding binding,
            const Ref<UniformBuffer>& buffer
        ) const;
        void UpdateDescriptorSet(
            SShaderBinding binding,
            const Ref<UniformBuffer>& buffer
        ) const;

        std::vector<VkDescriptorSet> m_DescriptorSets;
        std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;

        const VulkanGraphicsContext* m_GraphicsContext;
    };
}
