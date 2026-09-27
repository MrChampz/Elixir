#include "epch.h"
#include "PostProcessor.h"

#include <Engine/Graphics/CommandBuffer.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Graphics/SamplerBuilder.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir
{
    namespace
    {
        struct alignas(16) SPostProcessFrameData
        {
            glm::vec2 InverseSceneSize{};
            float BloomThreshold = 1.0f;
            float BloomKnee = 0.5f;
            float BloomRadius = 6.0f;
            float BloomIntensity = 0.08f;
            float Exposure = 0.0f;
            float Padding = 0.0f;
        };

        Ref<GraphicsPipeline> CreateFullscreenPipeline(
            const GraphicsContext* context,
            const Ref<Shader>& shader,
            const EImageFormat colorFormat
        )
        {
            PipelineBuilder builder;
            builder.SetShader(shader);
            builder.SetInputTopology(EPrimitiveTopology::TriangleList);
            builder.SetPolygonMode(EPolygonMode::Fill);
            builder.DisableBlending();
            builder.DisableDepthTest();
            builder.SetColorAttachmentFormat(colorFormat);
            builder.SetBufferLayout({});
            return builder.Build(context);
        }

        bool HasEqualExtent(const Extent3D& lhs, const Extent3D& rhs)
        {
            return lhs.Width == rhs.Width && lhs.Height == rhs.Height && lhs.Depth == rhs.Depth;
        }
    }

    PostProcessor::PostProcessor(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader
    ) : m_Context(context)
    {
        EE_CORE_ASSERT(m_Context, "PostProcessor requires a graphics context.")
        EE_CORE_ASSERT(shaderLoader, "PostProcessor requires a shader loader.")

        m_BloomShader = shaderLoader->LoadShader("./Shaders/", "PostProcessBloom");
        m_ToneMapShader = shaderLoader->LoadShader("./Shaders/", "PostProcessToneMap");

        EE_CORE_ASSERT(m_BloomShader, "PostProcessBloom shader could not be loaded.")
        EE_CORE_ASSERT(m_ToneMapShader, "PostProcessToneMap shader could not be loaded.")

        m_BloomPipeline = CreateFullscreenPipeline(
            m_Context,
            m_BloomShader,
            EImageFormat::R16G16B16A16_SFLOAT
        );
        m_ToneMapPipeline = CreateFullscreenPipeline(
            m_Context,
            m_ToneMapShader,
            EImageFormat::R8G8B8A8_SRGB
        );

        constexpr SPostProcessFrameData frameData{};
        m_FrameBuffer = UniformBuffer::Create(m_Context, sizeof(frameData), &frameData);
        m_Sampler = SamplerBuilder()
            .SetAddressModeU(ESamplerAddressMode::ClampToEdge)
            .SetAddressModeV(ESamplerAddressMode::ClampToEdge)
            .SetAddressModeW(ESamplerAddressMode::ClampToEdge)
            .Build(m_Context);

        m_BloomShader->BindConstantBuffer("cbPostProcess", m_FrameBuffer);
        m_BloomShader->BindSampler("postProcessSampler", m_Sampler);
        m_ToneMapShader->BindConstantBuffer("cbPostProcess", m_FrameBuffer);
        m_ToneMapShader->BindSampler("postProcessSampler", m_Sampler);
    }

    void PostProcessor::Apply(
        const Ref<Image>& sceneTarget,
        const Ref<Image>& renderTarget
    )
    {
        EE_CORE_ASSERT(sceneTarget, "Post processing requires a scene target.")
        EE_CORE_ASSERT(renderTarget, "Post processing requires a render target.")
        EE_CORE_ASSERT(
            HasEqualExtent(sceneTarget->GetExtent(), renderTarget->GetExtent()),
            "Post-process targets must have equal extents."
        )

        const auto extent = sceneTarget->GetExtent();
        EnsureBloomTarget(extent);

        const SPostProcessFrameData frameData{
            .InverseSceneSize = {
                1.0f / static_cast<float>(extent.Width),
                1.0f / static_cast<float>(extent.Height),
            },
            .BloomThreshold = m_Settings.BloomThreshold,
            .BloomKnee = m_Settings.BloomKnee,
            .BloomRadius = m_Settings.BloomRadius,
            .BloomIntensity = m_Settings.BloomIntensity,
            .Exposure = m_Settings.Exposure,
        };
        m_FrameBuffer->UpdateData(&frameData, sizeof(frameData));

        const SRenderingInfo bloomRenderingInfo{
            .ColorAttachment = m_BloomTarget,
            .RenderArea = extent,
        };

        const auto bloomCommandBuffer = m_Context->GetSecondaryCommandBuffer();
        bloomCommandBuffer->Begin(bloomRenderingInfo);

        m_BloomShader->BindImage("sceneTarget", sceneTarget);
        bloomCommandBuffer->BeginRendering(bloomRenderingInfo);
        bloomCommandBuffer->SetViewports({{
            .Width = static_cast<float>(extent.Width),
            .Height = static_cast<float>(extent.Height),
            .MaxDepth = 1.0f,
        }});
        bloomCommandBuffer->SetScissors({{ .Extent = { extent.Width, extent.Height } }});
        m_BloomPipeline->Bind(bloomCommandBuffer);
        bloomCommandBuffer->Draw(3);
        bloomCommandBuffer->EndRendering();

        bloomCommandBuffer->End();
        m_Context->EnqueueSecondaryCommandBuffer(bloomCommandBuffer);

        m_ToneMapShader->BindImage("sceneTarget", sceneTarget);
        m_ToneMapShader->BindImage("bloomTarget", m_BloomTarget);

        const SRenderingInfo toneMapRenderingInfo{
            .ColorAttachment = renderTarget,
            .RenderArea = extent,
        };

        const auto toneMapCommandBuffer = m_Context->GetSecondaryCommandBuffer();
        toneMapCommandBuffer->Begin(toneMapRenderingInfo);
        toneMapCommandBuffer->BeginRendering(toneMapRenderingInfo);
        toneMapCommandBuffer->SetViewports({{
            .Width = static_cast<float>(extent.Width),
            .Height = static_cast<float>(extent.Height),
            .MaxDepth = 1.0f,
        }});
        toneMapCommandBuffer->SetScissors({{ .Extent = { extent.Width, extent.Height } }});
        m_ToneMapPipeline->Bind(toneMapCommandBuffer);
        toneMapCommandBuffer->Draw(3);
        toneMapCommandBuffer->EndRendering();

        toneMapCommandBuffer->End();
        m_Context->EnqueueSecondaryCommandBuffer(toneMapCommandBuffer);
    }

    void PostProcessor::EnsureBloomTarget(const Extent3D& extent)
    {
        if (m_BloomTarget && HasEqualExtent(m_BloomTarget->GetExtent(), extent))
            return;

        if (m_BloomTarget)
        {
            m_BloomTarget->Resize(m_Context->GetUploadCommandBuffer(), extent);
            return;
        }

        m_BloomTarget = Image::Create(m_Context, {
            .Width = extent.Width,
            .Height = extent.Height,
            .Depth = extent.Depth,
            .Type = EImageType::_2D,
            .Format = EImageFormat::R16G16B16A16_SFLOAT,
            .Usage = EImageUsage::ColorAttachment | EImageUsage::Sampled |
                EImageUsage::TransferSrc | EImageUsage::TransferDst,
            .InitialLayout = EImageLayout::General,
        });
        EE_CORE_ASSERT(m_BloomTarget, "PostProcessor could not create its bloom target.")
    }
}
