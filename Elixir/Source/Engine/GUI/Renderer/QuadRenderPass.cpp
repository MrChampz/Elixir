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
    ) : m_GeometrySlots(*context), m_DPIScale(dpiScale), m_PerFrameConstantBuffer(perFrameCB), m_GraphicsContext(context)
    {
        EE_CORE_TRACE("Initializing GUI: QuadRenderPass.")
        InitRenderPass(shaderLoader);
        if (!m_Shader || !m_Pipeline) return;
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
        m_GeometrySlots.MarkChanged();
    }

    uint32_t QuadRenderPass::AppendRange(const std::span<const SDrawCommand> commands)
    {
        if (!m_Shader || !m_Pipeline) return 0;

        const auto firstInstance = (uint32_t)m_Quads.size();

        for (const auto& drawCmd : commands)
            BuildRectGeometry(drawCmd);

        return firstInstance;
    }

    void QuadRenderPass::Bind(const Ref<CommandBuffer>& cmd)
    {
        m_CanRender = false;
        if (!m_Shader || !m_Pipeline) return;
        m_Pipeline->Bind(cmd);
        auto* geometry = m_GeometrySlots.UpdateCurrent([this](SGeometryBuffer& slot)
        {
            if (m_Quads.empty()) return true;
            if (!slot.Buffer || m_Quads.size() > slot.Capacity)
            {
                const size_t capacity = GrowBufferCapacity(slot.Capacity, m_Quads.size());
                auto buffer = DynamicVertexBuffer::Create(m_GraphicsContext, capacity * sizeof(SQuad));
                if (!buffer || !buffer->IsValid())
                {
                    EE_CORE_ERROR("Failed to allocate GUI geometry buffer.")
                    return false;
                }
                buffer->SetLayout(m_BufferLayout);
                // The current slot's fence has completed; its previous buffer can be released.
                slot.Buffer = std::move(buffer);
                slot.Capacity = capacity;
            }
            slot.Buffer->UpdateData(m_Quads.data(), m_Quads.size() * sizeof(SQuad));
            return true;
        });
        m_CanRender = geometry && geometry->Buffer;
        if (m_CanRender) geometry->Buffer->Bind(cmd);
    }

    void QuadRenderPass::Render(
        const Ref<CommandBuffer>& cmd,
        const uint32_t firstInstance,
        const uint32_t instanceCount
    )
    {
        if (!m_CanRender) return;
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
                    { EDataType::Float2,  "Position"          },
                    { EDataType::Float2,  "Size"              },
                    { EDataType::Float4,  "Border"            },
                    { EDataType::Float4,  "InsetShadow"       },
                    { EDataType::Float4,  "DropShadow"        },
                    { EDataType::Float4,  "Color"             },
                    { EDataType::Float4,  "OutlineColor"      },
                    { EDataType::Float,   "OutlineThickness"  },
                    { EDataType::UInt,    "TextureIndex"      },
                    { EDataType::UInt,    "TextureMapping"    },
                    { EDataType::Float4,  "ClipRect"       },
                    { EDataType::Float4,  "RoundedClipRect"   },
                    { EDataType::Float4,  "ClipCornerRadius"  },
                },
                EInputRate::Instance
            }
        });

        m_Shader = shaderLoader->LoadShader("./Shaders/", "GUI");
        if (!m_Shader)
        {
            EE_CORE_ERROR("Could not load the GUI shader; disabling the GUI quad pass.");
            return;
        }

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
        m_BufferLayout = bufferLayout;

        m_WhiteTexture = Texture2D::Create(
            m_GraphicsContext,
            EImageFormat::R8G8B8A8_SRGB,
            1, 1,
            &Color::WhiteAlpha
        );

        m_ImageSet = ImageSet::Create(m_GraphicsContext);
        m_WhiteTextureHandle = m_ImageSet->AddTexture(m_WhiteTexture);
    }

    void QuadRenderPass::BindShaderParameters() const
    {
        m_Shader->BindConstantBuffer("cbPerFrame", m_PerFrameConstantBuffer);
        m_Shader->BindImageSet("textures", m_ImageSet);
        m_Shader->SetPushConstant("pcWhiteTexture", (void*)&m_WhiteTextureHandle.Index, sizeof(uint32_t));

        const auto sampler = SamplerBuilder()
            .SetMagFilter(ESamplerFilter::Linear)
            .SetMinFilter(ESamplerFilter::Linear)
            .Build(m_GraphicsContext);
        m_Shader->BindSampler("samplerState", sampler);
    }

    void QuadRenderPass::BuildRectGeometry(const SDrawCommand& cmd)
    {
        const auto imageHandle = cmd.Image
            ? m_ImageSet->AddImage(cmd.Image)
            : m_WhiteTextureHandle;

        const SQuad quad = {
            .Position = cmd.Geometry.Position * m_DPIScale,
            .Size = cmd.Geometry.Size * m_DPIScale,
            .Border = cmd.Border * m_DPIScale,
            .InsetShadow = cmd.InsetShadow * m_DPIScale,
            .DropShadow = cmd.DropShadow * m_DPIScale,
            .Color = cmd.Color,
            .OutlineColor = cmd.Outline.Color,
            .OutlineThickness = cmd.Outline.Thickness * m_DPIScale,
            .TextureIndex = imageHandle.IsValid()
                ? imageHandle.Index
                : m_WhiteTextureHandle.Index,
            .TextureMapping = (uint32_t)cmd.TextureMapping,
            .ClipRect = cmd.ClipRect.IsValid()
                ? cmd.ClipRect * m_DPIScale
                : cmd.ClipRect,
            .RoundedClipRect = cmd.RoundedClipRect * m_DPIScale,
            .ClipCornerRadius = cmd.ClipCornerRadius * m_DPIScale,
        };

        m_Quads.push_back(quad);
    }
}
