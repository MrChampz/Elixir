#include "epch.h"
#include "QuadRenderPass.h"

#include "Engine/Graphics/TextureLoader.h"

#include <Engine/Core/Color.h>
#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Graphics/SamplerBuilder.h>

namespace Elixir::GUI
{
    QuadRenderPass::QuadRenderPass(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const float dpiScale,
        const Ref<UniformBuffer>& perFrameCB
    ) : m_DPIScale(dpiScale), m_PerFrameConstantBuffer(perFrameCB), m_GraphicsContext(context)
    {
        EE_CORE_TRACE("Initializing GUI: QuadRenderPass.")
        InitRenderPass(shaderLoader);
        BindShaderParameters();
    }

    QuadRenderPass::~QuadRenderPass()
    {
        m_Quads.clear();
        m_WhiteTexture.reset();
    }

    void QuadRenderPass::GenerateDrawCommands(const RenderBatch& batch)
    {
        m_Quads.clear();

        for (const auto& drawCmd : batch.GetCommands())
        {
            switch (drawCmd.Type)
            {
                case SDrawCommand::EType::Rect:
                    BuildRectGeometry(drawCmd);
                    break;
                default:
                    break;
            }
        }

        if (!m_Quads.empty())
        {
            m_QuadBuffer->UpdateData(m_Quads.data(),  m_Quads.size() * sizeof(SQuad));
        }
    }

    void QuadRenderPass::Render(const Ref<CommandBuffer>& cmd)
    {
        m_Pipeline->Bind(cmd);
        m_QuadBuffer->Bind(cmd);
        cmd->Draw(6, m_Quads.size());
    }

    bool QuadRenderPass::HasData() const
    {
        return !m_Quads.empty();
    }

    void QuadRenderPass::Clear()
    {
        m_Quads.clear();
    }

    void QuadRenderPass::InitRenderPass(const ShaderLoader* shaderLoader)
    {
        const BufferLayout bufferLayout({
            {
                {
                    { EDataType::Vec2,  "Position"          },
                    { EDataType::Vec2,  "Size"              },
                    { EDataType::Vec4,  "Border"            },
                    { EDataType::Vec4,  "InsetShadow"       },
                    { EDataType::Vec4,  "DropShadow"        },
                    { EDataType::Vec4,  "Color"             },
                    { EDataType::Vec4,  "OutlineColor"      },
                    { EDataType::Float, "OutlineThickness"  },
                    { EDataType::UInt,  "TextureIndex"      },
                    { EDataType::Vec4,  "ScissorRect"       },
                },
                EInputRate::Instance
            }
        });

        m_Shader = shaderLoader->LoadShader("./Shaders/", "GUI");

        PipelineBuilder builder;
        builder.SetShader(m_Shader);
        builder.SetInputTopology(EPrimitiveTopology::TriangleList);
        builder.SetPolygonMode(EPolygonMode::Fill);
        builder.SetCullMode(ECullMode::Back, EFrontFace::CounterClockwise);
        builder.EnableAlphaBlending();
        builder.DisableDepthTest();
        builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        builder.SetBufferLayout(bufferLayout);
        m_Pipeline = builder.Build(m_GraphicsContext);

        m_Quads.reserve(MAX_QUADS);
        m_QuadBuffer = DynamicVertexBuffer::Create(m_GraphicsContext, MAX_QUADS * sizeof(SQuad));
        m_QuadBuffer->SetLayout(bufferLayout);

        m_WhiteTexture = Texture2D::Create(
            m_GraphicsContext,
            EImageFormat::R8G8B8A8_SRGB,
            1, 1,
            &Color::WhiteAlpha
        );

        m_TextureSet = TextureSet::Create(m_GraphicsContext);
        m_WhiteTextureHandle = m_TextureSet->AddTexture(m_WhiteTexture);
    }

    void QuadRenderPass::BindShaderParameters() const
    {
        m_Shader->BindConstantBuffer("cbPerFrame", m_PerFrameConstantBuffer);
        m_Shader->BindTextureSet("textures", m_TextureSet);
        m_Shader->SetPushConstant("pcWhiteTexture", (void*)&m_WhiteTextureHandle.Index, sizeof(uint32_t));

        const auto sampler = SamplerBuilder()
            .SetMagFilter(ESamplerFilter::Linear)
            .SetMinFilter(ESamplerFilter::Linear)
            .Build(m_GraphicsContext);
        m_Shader->BindSampler("samplerState", sampler);
    }

    void QuadRenderPass::BuildRectGeometry(const SDrawCommand& cmd)
    {
        const SQuad quad = {
            .Position = cmd.Geometry.Position * m_DPIScale,
            .Size = cmd.Geometry.Size * m_DPIScale,
            .Border = cmd.Border * m_DPIScale,
            .InsetShadow = cmd.InsetShadow * m_DPIScale,
            .DropShadow = cmd.DropShadow * m_DPIScale,
            .Color = cmd.Color,
            .OutlineColor = cmd.Outline.Color,
            .OutlineThickness = cmd.Outline.Thickness * m_DPIScale,
            .TextureIndex = cmd.Texture
                ? m_TextureSet->AddTexture(cmd.Texture).Index
                : m_WhiteTextureHandle.Index,
            .ScissorRect = cmd.ScissorRect.IsValid()
                ? cmd.ScissorRect * m_DPIScale
                : cmd.ScissorRect
        };

        m_Quads.push_back(quad);
    }
}