#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Graphics/GraphicsTypes.h>
#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/Shader/Shader.h>

namespace Elixir
{
    struct SPipelineInputAssemblyInfo
    {
        EPrimitiveTopology Topology;
        bool PrimitiveRestartEnable;
    };

    struct SPipelineViewportInfo
    {
        std::vector<Viewport> Viewports;
        std::vector<Rect2D> Scissors;
    };

    struct SPipelineRasterizationInfo
    {
        bool DepthClampEnable;
        bool RasterizerDiscardEnable;
        EPolygonMode PolygonMode;
        ECullMode CullMode;
        EFrontFace FrontFace;
        bool DepthBiasEnable;
        float DepthBiasConstantFactor;
        float DepthBiasClamp;
        float DepthBiasSlopeFactor;
        float LineWidth;
    };

    struct SPipelineMultisampleInfo
    {
        bool SampleShadingEnable;
        ESampleCount RasterizationSamples;
        float MinSampleShading;
        const SampleMask* SampleMask;
        bool AlphaToCoverageEnable;
        bool AlphaToOneEnable;
    };

    struct SColorBlendAttachment
    {
        bool BlendEnable = false;
        EBlendFactor SrcColorBlendFactor;
        EBlendFactor DstColorBlendFactor;
        EBlendOp ColorBlendOp;
        EBlendFactor SrcAlphaBlendFactor;
        EBlendFactor DstAlphaBlendFactor;
        EBlendOp AlphaBlendOp;
        EColorComponent ColorWriteMask = EColorComponent::R |
            EColorComponent::G | EColorComponent::B | EColorComponent::A;;
    };

    struct SPipelineColorBlendInfo
    {
        bool LogicOpEnable;
        ELogicOp LogicOp;
        std::vector<SColorBlendAttachment> Attachments;
        float BlendConstants[4];
    };

    struct SStencilOpState
    {
        EStencilOp FailOp;
        EStencilOp PassOp;
        EStencilOp DepthFailOp;
        ECompareOp CompareOp;
        uint32_t CompareMask;
        uint32_t WriteMask;
        uint32_t Reference;
    };

    struct SPipelineDepthStencilInfo
    {
        bool DepthTestEnable;
        bool DepthWriteEnable;
        ECompareOp DepthCompareOp;
        bool DepthBoundsTestEnable;
        bool StencilTestEnable;
        SStencilOpState Front;
        SStencilOpState Back;
        float MinDepthBounds;
        float MaxDepthBounds;
    };

    struct SPipelineCreateInfo
    {
        SPipelineInputAssemblyInfo InputAssembly;
        SPipelineViewportInfo Viewport;
        SPipelineRasterizationInfo Rasterization;
        SPipelineMultisampleInfo Multisample;
        SPipelineColorBlendInfo ColorBlend;
        SPipelineDepthStencilInfo DepthStencil;
        EImageFormat ColorAttachmentFormat;
        EDepthStencilImageFormat DepthAttachmentFormat = EDepthStencilImageFormat::Undefined;
        Ref<Shader> Shader;
        BufferLayout VertexBufferLayout;
    };

    class ELIXIR_API Pipeline
    {
    public:
        virtual ~Pipeline() = default;

        virtual void Bind(const Ref<CommandBuffer>& cmd) = 0;

        const Ref<Shader>& GetShader() const { return m_Shader; }

      protected:
        explicit Pipeline(const SPipelineCreateInfo& info) : m_Shader(info.Shader) {}

        const Ref<Shader> m_Shader;
    };

    class ELIXIR_API GraphicsPipeline : public Pipeline
    {
      public:
        ~GraphicsPipeline() override = default;

        static Ref<GraphicsPipeline> Create(
            const GraphicsContext* context,
            const SPipelineCreateInfo& info
        );

      protected:
        GraphicsPipeline(const GraphicsContext* context, const SPipelineCreateInfo& info);

        SPipelineInputAssemblyInfo m_InputAssembly;
        SPipelineViewportInfo m_Viewport;
        SPipelineRasterizationInfo m_Rasterization;
        SPipelineMultisampleInfo m_Multisample;
        SPipelineColorBlendInfo m_ColorBlend;
        SPipelineDepthStencilInfo m_DepthStencil;
        EImageFormat m_ColorAttachmentFormat;
        EImageFormat m_DepthAttachmentFormat;

        BufferLayout m_BufferLayout;

        const GraphicsContext* m_GraphicsContext;
    };

    class ELIXIR_API ComputePipeline : public Pipeline
    {
      public:
        ~ComputePipeline() override = default;

        static Ref<ComputePipeline> Create(
            const GraphicsContext* context,
            const SPipelineCreateInfo& info
        );

      protected:
        ComputePipeline(const GraphicsContext* context, const SPipelineCreateInfo& info);

        const GraphicsContext* m_GraphicsContext;
    };
}
