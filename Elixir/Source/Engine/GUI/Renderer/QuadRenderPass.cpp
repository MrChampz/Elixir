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

    void QuadRenderPass::BeginFrame()
    {
        m_Quads.clear();
    }

    void QuadRenderPass::EndFrame()
    {
        if (!m_Quads.empty())
        {
            EnsureQuadBufferCapacity(m_Quads.size());
            m_QuadBuffer->UpdateData(m_Quads.data(),  m_Quads.size() * sizeof(SQuad));
        }
    }

    uint32_t QuadRenderPass::AppendRange(const std::span<const SDrawCommand> commands)
    {
        const auto firstInstance = (uint32_t)m_Quads.size();

        for (const auto& drawCmd : commands)
            BuildRectGeometry(drawCmd);

        return firstInstance;
    }

    void QuadRenderPass::Bind(const Ref<CommandBuffer>& cmd)
    {
        m_Pipeline->Bind(cmd);
        m_QuadBuffer->Bind(cmd);
    }

    void QuadRenderPass::Render(
        const Ref<CommandBuffer>& cmd,
        const uint32_t firstInstance,
        const uint32_t instanceCount
    )
    {
        cmd->Draw(6, instanceCount, 0, firstInstance);
    }

    bool QuadRenderPass::HasData() const
    {
        return !m_Quads.empty();
    }

    void QuadRenderPass::Clear()
    {
        m_Quads.clear();
    }

    uint32_t QuadRenderPass::GetInstanceCount() const
    {
        return (uint32_t)m_Quads.size();
    }

    EDrawCommandType QuadRenderPass::GetHandleType() const
    {
        return EDrawCommandType::Rect;
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
                    { EDataType::UInt,  "TextureMapping"    },
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
        m_QuadCapacity = MAX_QUADS;

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

    void QuadRenderPass::EnsureQuadBufferCapacity(const size_t requiredCapacity)
    {
        if (requiredCapacity <= m_QuadCapacity)
            return;

        const size_t newCapacity = GrowBufferCapacity(m_QuadCapacity, requiredCapacity);
        auto buffer = DynamicVertexBuffer::Create(m_GraphicsContext, newCapacity * sizeof(SQuad));
        buffer->SetLayout(m_QuadBuffer->GetLayout());

        m_RetiredQuadBuffers.push_back(std::move(m_QuadBuffer));
        m_QuadBuffer = std::move(buffer);
        m_QuadCapacity = newCapacity;
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
            .TextureMapping = (uint32_t)cmd.TextureMapping,
            .ScissorRect = cmd.ScissorRect.IsValid()
                ? cmd.ScissorRect * m_DPIScale
                : cmd.ScissorRect
        };

        m_Quads.push_back(quad);
    }
}
