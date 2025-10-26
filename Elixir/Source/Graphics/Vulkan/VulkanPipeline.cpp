#include "epch.h"
#include "VulkanPipeline.h"

#include "Utils.h"
#include "VulkanShader.h"
#include "VulkanShaderModule.h"

namespace Elixir::Vulkan
{
    VulkanGraphicsPipeline::VulkanGraphicsPipeline(
        const GraphicsContext* context,
        const SPipelineCreateInfo& info
    ) : GraphicsPipeline(context, info), m_Pipeline(VK_NULL_HANDLE)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);
        BindShader();
        CreatePipeline();
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
    {
        EE_PROFILE_ZONE_SCOPED()

        vkDestroyPipeline(
            m_GraphicsContext->GetDevice(),
            m_Pipeline,
            nullptr
        );

        m_ShaderStages.clear();
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    void VulkanGraphicsPipeline::BindShader()
    {
        const auto vkShader = static_pointer_cast<VulkanShader>(m_Shader);

        m_PipelineLayout = vkShader->GetPipelineLayout();

        for (const auto module : m_Shader->GetModules())
        {
            const auto vkModule = static_pointer_cast<VulkanShaderModule>(module);

            VkPipelineShaderStageCreateInfo stageInfo = {};
            stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage = Converters::GetShaderStage(module->GetStage());
            stageInfo.module = vkModule->GetVulkanShaderModule();
            stageInfo.pName = module->GetEntrypoint().c_str();

            m_ShaderStages.push_back(stageInfo);
        }
    }

    void VulkanGraphicsPipeline::CreatePipeline()
    {
        auto colorAttachmentFormats = { Converters::GetFormat(m_ColorAttachmentFormat) };

        VkPipelineRenderingCreateInfo renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingInfo.colorAttachmentCount = colorAttachmentFormats.size();
        renderingInfo.pColorAttachmentFormats = colorAttachmentFormats.begin();
        renderingInfo.depthAttachmentFormat = Converters::GetFormat(m_DepthAttachmentFormat);

        VkGraphicsPipelineCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.pNext = &renderingInfo;

        info.stageCount = (uint32_t)m_ShaderStages.size();
        info.pStages = m_ShaderStages.data();

        const auto vertexInputState = CreateVertexInputState();
        info.pVertexInputState = &vertexInputState;

        const auto inputAssemblyState = CreateInputAssemblyState();
        info.pInputAssemblyState = &inputAssemblyState;

        const auto viewportState = CreateViewportState();
        info.pViewportState = &viewportState;

        const auto rasterizationState = CreateRasterizationState();
        info.pRasterizationState = &rasterizationState;

        const auto multisampleState = CreateMultisampleState();
        info.pMultisampleState = &multisampleState;

        const auto colorBlendState = CreateColorBlendState();
        info.pColorBlendState = &colorBlendState;

        const auto depthStencilState = CreateDepthStencilState();
        info.pDepthStencilState = &depthStencilState;

        const auto dynamicState = CreateDynamicState();
        info.pDynamicState = &dynamicState;

        info.layout = m_PipelineLayout;

        VK_CHECK_RESULT(
            vkCreateGraphicsPipelines(
                m_GraphicsContext->GetDevice(),
                VK_NULL_HANDLE,
                1,
                &info,
                nullptr,
                &m_Pipeline
            )
        );
    }

    VkPipelineVertexInputStateCreateInfo VulkanGraphicsPipeline::CreateVertexInputState()
    {
        m_Binding.binding = 0;
        m_Binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        m_Binding.stride = m_BufferLayout.GetStride();

        m_Attributes.resize(m_BufferLayout.GetElements().size());

        uint32_t location = 0;
        for (const auto& element : m_BufferLayout)
        {
            VkVertexInputAttributeDescription attribute = {};
            attribute.binding = m_Binding.binding;
            attribute.location = location;
            attribute.format = Converters::GetFormat(element.Type);
            attribute.offset = element.Offset;

            m_Attributes[location] = attribute;
            location++;
        }

        VkPipelineVertexInputStateCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        info.pNext = nullptr;
        info.vertexBindingDescriptionCount = 1;
        info.pVertexBindingDescriptions = &m_Binding;
        info.vertexAttributeDescriptionCount = (uint32_t)m_Attributes.size();
        info.pVertexAttributeDescriptions = m_Attributes.data();

        return info;
    }

    VkPipelineInputAssemblyStateCreateInfo VulkanGraphicsPipeline::CreateInputAssemblyState() const
    {
        VkPipelineInputAssemblyStateCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        info.pNext = nullptr;
        info.topology = Converters::GetPrimitiveTopology(m_InputAssembly.Topology);
        info.primitiveRestartEnable = m_InputAssembly.PrimitiveRestartEnable;

        return info;
    }

    VkPipelineViewportStateCreateInfo VulkanGraphicsPipeline::CreateViewportState()
    {
        VkPipelineViewportStateCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        info.pNext = nullptr;
        info.viewportCount = 1;
        info.scissorCount = 1;
        info.pViewports = nullptr;
        info.pScissors = nullptr;

        return info;
    }

    VkPipelineRasterizationStateCreateInfo VulkanGraphicsPipeline::CreateRasterizationState() const
    {
        VkPipelineRasterizationStateCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        info.depthClampEnable = m_Rasterization.DepthClampEnable;
        info.rasterizerDiscardEnable = m_Rasterization.RasterizerDiscardEnable;
        info.polygonMode = Converters::GetPolygonMode(m_Rasterization.PolygonMode);
        info.cullMode = Converters::GetCullMode(m_Rasterization.CullMode);
        info.frontFace = Converters::GetFrontFace(m_Rasterization.FrontFace);
        info.depthBiasEnable = m_Rasterization.DepthBiasEnable;
        info.depthBiasConstantFactor = m_Rasterization.DepthBiasConstantFactor;
        info.depthBiasClamp = m_Rasterization.DepthBiasClamp;
        info.depthBiasSlopeFactor = m_Rasterization.DepthBiasSlopeFactor;
        info.lineWidth = m_Rasterization.LineWidth;

        return info;
    }

    VkPipelineMultisampleStateCreateInfo VulkanGraphicsPipeline::CreateMultisampleState() const
    {
        VkPipelineMultisampleStateCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        info.sampleShadingEnable = m_Multisample.SampleShadingEnable;
        info.rasterizationSamples = Converters::GetSampleCount(m_Multisample.RasterizationSamples);
        info.minSampleShading = m_Multisample.MinSampleShading;
        info.pSampleMask = m_Multisample.SampleMask;
        info.alphaToCoverageEnable = m_Multisample.AlphaToCoverageEnable;
        info.alphaToOneEnable = m_Multisample.AlphaToOneEnable;

        return info;
    }

    VkPipelineColorBlendStateCreateInfo VulkanGraphicsPipeline::CreateColorBlendState() const
    {
        std::vector<VkPipelineColorBlendAttachmentState> attachments;
        attachments.reserve(m_ColorBlend.Attachments.size());

        for (const auto& attachment : m_ColorBlend.Attachments)
        {
            VkPipelineColorBlendAttachmentState state = {};
            state.blendEnable = attachment.BlendEnable;
            state.srcColorBlendFactor = Converters::GetBlendFactor(attachment.SrcColorBlendFactor);
            state.dstColorBlendFactor = Converters::GetBlendFactor(attachment.DstColorBlendFactor);
            state.colorBlendOp = Converters::GetBlendOp(attachment.ColorBlendOp);
            state.srcAlphaBlendFactor = Converters::GetBlendFactor(attachment.SrcAlphaBlendFactor);
            state.dstAlphaBlendFactor = Converters::GetBlendFactor(attachment.DstAlphaBlendFactor);
            state.alphaBlendOp = Converters::GetBlendOp(attachment.AlphaBlendOp);
            state.colorWriteMask = Converters::GetColorComponents(attachment.ColorWriteMask);

            attachments.push_back(state);
        }

        VkPipelineColorBlendStateCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        info.pNext = nullptr;
        info.logicOpEnable = m_ColorBlend.LogicOpEnable;
        info.logicOp = Converters::GetLogicOp(m_ColorBlend.LogicOp);
        info.attachmentCount = (uint32_t)attachments.size();
        info.pAttachments = attachments.data();
        (*info.blendConstants) = m_ColorBlend.BlendConstants[0];

        return info;
    }

    VkPipelineDepthStencilStateCreateInfo VulkanGraphicsPipeline::CreateDepthStencilState() const
    {
        VkPipelineDepthStencilStateCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        info.pNext = nullptr;
        info.depthTestEnable = m_DepthStencil.DepthTestEnable;
        info.depthWriteEnable = m_DepthStencil.DepthWriteEnable;
        info.depthCompareOp = Converters::GetCompareOp(m_DepthStencil.DepthCompareOp);
        info.depthBoundsTestEnable = m_DepthStencil.DepthBoundsTestEnable;
        info.stencilTestEnable = m_DepthStencil.StencilTestEnable;
        info.front = Converters::GetStencilOpState(m_DepthStencil.Front);
        info.back = Converters::GetStencilOpState(m_DepthStencil.Back);
        info.minDepthBounds = m_DepthStencil.MinDepthBounds;
        info.maxDepthBounds = m_DepthStencil.MaxDepthBounds;

        return info;
    }

    VkPipelineDynamicStateCreateInfo VulkanGraphicsPipeline::CreateDynamicState()
    {
        VkPipelineDynamicStateCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        info.pDynamicStates = m_DynamicState.data();
        info.dynamicStateCount = m_DynamicState.size();

        return info;
    }
}