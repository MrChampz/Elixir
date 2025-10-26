#include "epch.h"
#include "PipelineBuilder.h"

namespace Elixir
{
    PipelineBuilder::PipelineBuilder()
    {
        Clear();
    }

    void PipelineBuilder::SetInputTopology(const EPrimitiveTopology topology)
    {
        m_InputAssembly.Topology = topology;
        m_InputAssembly.PrimitiveRestartEnable = false;
    }

    void PipelineBuilder::SetPolygonMode(const EPolygonMode mode)
    {
        m_Rasterization.PolygonMode = mode;
        m_Rasterization.LineWidth = 1.0f;
    }

    void PipelineBuilder::SetCullMode(const ECullMode mode, const EFrontFace frontFace)
    {
        m_Rasterization.CullMode = mode;
        m_Rasterization.FrontFace = frontFace;
    }

    void PipelineBuilder::SetMultisamplingNone()
    {
        m_Multisample.SampleShadingEnable = false;
        m_Multisample.RasterizationSamples = ESampleCount::_1;
        m_Multisample.MinSampleShading = 1.0f;
        m_Multisample.SampleMask = nullptr;
        m_Multisample.AlphaToCoverageEnable = false;
        m_Multisample.AlphaToOneEnable = false;
    }

    void PipelineBuilder::DisableBlending()
    {
        m_ColorBlendAttachment.BlendEnable = false;
        m_ColorBlendAttachment.ColorWriteMask = EColorComponent::R |
            EColorComponent::G | EColorComponent::B | EColorComponent::A;
    }

    void PipelineBuilder::DisableDepthTest()
    {
        m_DepthStencil.DepthTestEnable = false;
        m_DepthStencil.DepthWriteEnable = false;
        m_DepthStencil.DepthCompareOp = ECompareOp::Never;
        m_DepthStencil.DepthBoundsTestEnable = false;
        m_DepthStencil.StencilTestEnable = false;
        m_DepthStencil.Front = {};
        m_DepthStencil.Back = {};
        m_DepthStencil.MinDepthBounds = 0.0f;
        m_DepthStencil.MaxDepthBounds = 1.0f;
    }

    void PipelineBuilder::SetColorAttachmentFormat(const EImageFormat format)
    {
        m_ColorAttachmentFormat = format;
    }

    void PipelineBuilder::SetDepthAttachmentFormat(const EDepthStencilImageFormat format)
    {
        m_DepthAttachmentFormat = format;
    }

    void PipelineBuilder::SetShader(const Ref<Shader>& shader)
    {
        m_Shader = shader;
    }

    void PipelineBuilder::SetBufferLayout(const BufferLayout& layout)
    {
        m_VertexBufferLayout = layout;
    }

    void PipelineBuilder::Clear()
    {
        m_InputAssembly = {};
        m_Rasterization = {};
        m_Multisample = {};
        m_DepthStencil = {};
        m_ColorBlendAttachment = {};
        m_Shader = nullptr;
        m_VertexBufferLayout = {};
    }

    Ref<GraphicsPipeline> PipelineBuilder::Build(const GraphicsContext* context) const
    {
        SPipelineColorBlendInfo colorBlend{};
        colorBlend.LogicOpEnable = false;
        colorBlend.LogicOp = ELogicOp::Copy;
        colorBlend.Attachments = { m_ColorBlendAttachment };

        SPipelineCreateInfo info{};
        info.InputAssembly = m_InputAssembly;
        info.Rasterization = m_Rasterization;
        info.Multisample = m_Multisample;
        info.ColorBlend = colorBlend;
        info.DepthStencil = m_DepthStencil;
        info.ColorAttachmentFormat = m_ColorAttachmentFormat;
        info.DepthAttachmentFormat = m_DepthAttachmentFormat;
        info.Shader = m_Shader;
        info.VertexBufferLayout = m_VertexBufferLayout;

        return GraphicsPipeline::Create(context, info);
    }
}