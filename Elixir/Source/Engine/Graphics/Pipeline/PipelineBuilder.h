#pragma once

#include <Engine/Graphics/Pipeline/Pipeline.h>

namespace Elixir
{
    class ELIXIR_API PipelineBuilder
    {
      public:
        PipelineBuilder();

        void SetInputTopology(EPrimitiveTopology topology);
        void SetPolygonMode(EPolygonMode mode);
        void SetCullMode(ECullMode mode, EFrontFace frontFace);
        void SetMultisamplingNone();
        void EnableAlphaBlending();
        void DisableBlending();
        void DisableDepthTest();
        void SetColorAttachmentFormat(EImageFormat format);
        void SetDepthAttachmentFormat(EDepthStencilImageFormat format);
        void SetShader(const Ref<Shader>& shader);
        void SetBufferLayout(const BufferLayout& layout);

        void Clear();

        SPipelineCreateInfo GetCreateInfo() const;
        Ref<GraphicsPipeline> Build(const GraphicsContext* context) const;

      protected:
        SPipelineInputAssemblyInfo m_InputAssembly;
        SPipelineRasterizationInfo m_Rasterization;
        SPipelineMultisampleInfo m_Multisample;
        SColorBlendAttachment m_ColorBlendAttachment;
        SPipelineDepthStencilInfo m_DepthStencil;

        EImageFormat m_ColorAttachmentFormat;
        EDepthStencilImageFormat m_DepthAttachmentFormat;

        Ref<Shader> m_Shader;
        BufferLayout m_VertexBufferLayout;
    };
}
