#pragma once

#include <Engine/Graphics/Shader/Shader.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

namespace Elixir::Vulkan
{
    using namespace Elixir;

    class ELIXIR_API VulkanShader : public Shader
    {
      public:
        ~VulkanShader() override;

      protected:
        VulkanShader(const GraphicsContext* context, SShaderCreateInfo& info);

        void CreateDescriptorSetLayouts();
        void CreateDescriptorSets();

        void UpdateDescriptorSets();

        VkWriteDescriptorSet GetWriteDescriptorSet(
            const SShaderBinding& binding,
            const Ref<Texture>& texture
        ) const;
        void UpdateDescriptorSet(
            const SShaderBinding& binding,
            const Ref<Texture>& texture
        ) const;

        VkWriteDescriptorSet GetWriteDescriptorSet(
            const SShaderBinding& binding,
            const Ref<UniformBuffer>& buffer
        ) const;
        void UpdateDescriptorSet(
            const SShaderBinding& binding,
            const Ref<UniformBuffer>& buffer
        ) const;

        std::vector<VkDescriptorSet> m_DescriptorSets;
        std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;

        const VulkanGraphicsContext* m_GraphicsContext;
    };
}
