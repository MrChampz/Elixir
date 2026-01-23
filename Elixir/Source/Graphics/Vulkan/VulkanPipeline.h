#pragma once

#include <Engine/Graphics/Pipeline/Pipeline.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

#include <vulkan/vulkan.h>

namespace Elixir::Vulkan
{
    using namespace Elixir;

    class ELIXIR_API VulkanGraphicsPipeline final : public GraphicsPipeline
    {
      public:
        VulkanGraphicsPipeline(const GraphicsContext* context, const SPipelineCreateInfo& info);
        ~VulkanGraphicsPipeline() override;

        void Bind(const Ref<CommandBuffer>& cmd) override;

        VkPipeline GetVulkanPipeline() const { return m_Pipeline; }

      protected:
        void BindShader();
        void CreatePipeline();

        VkPipelineVertexInputStateCreateInfo CreateVertexInputState();
        VkPipelineInputAssemblyStateCreateInfo CreateInputAssemblyState() const;
        VkPipelineViewportStateCreateInfo CreateViewportState();
        VkPipelineRasterizationStateCreateInfo CreateRasterizationState() const;
        VkPipelineMultisampleStateCreateInfo CreateMultisampleState() const;
        VkPipelineColorBlendStateCreateInfo CreateColorBlendState();
        VkPipelineDepthStencilStateCreateInfo CreateDepthStencilState() const;
        VkPipelineDynamicStateCreateInfo CreateDynamicState() const;

        VkPipeline m_Pipeline;
        VkPipelineLayout m_PipelineLayout;

        std::vector<VkVertexInputBindingDescription> m_Bindings;
        std::vector<VkVertexInputAttributeDescription> m_Attributes;

        std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStages;

        std::vector<VkPipelineColorBlendAttachmentState> m_ColorBlendAttachments;

        std::array<VkDynamicState, 2> m_DynamicState =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        const VulkanGraphicsContext* m_GraphicsContext;
    };
}
