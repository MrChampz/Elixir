#include "epch.h"
#include "DebugRenderPass.h"

#include <Engine/Core/Color.h>
#include <Engine/Graphics/Pipeline/PipelineBuilder.h>

namespace Elixir::GUI
{
    DebugRenderPass::DebugRenderPass(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const float dpiScale,
        const Ref<UniformBuffer>& perFrameCB
    ) : m_GeometrySlots(*context), m_DPIScale(dpiScale), m_PerFrameConstantBuffer(perFrameCB), m_GraphicsContext(context)
    {
        EE_CORE_TRACE("Initializing GUI: DebugRenderPass.")
        InitRenderPass(shaderLoader);
        BindShaderParameters();
    }

    void DebugRenderPass::BeginFrame()
    {
        m_Vertices.clear();
    }

    void DebugRenderPass::EndFrame()
    {
        m_GeometrySlots.MarkChanged();
    }

    uint32_t DebugRenderPass::AppendRange(std::span<const SDrawCommand> commands)
    {
        const auto firstVertex = (uint32_t)m_Vertices.size();

        for (const auto& drawCmd : commands)
            BuildDebugRectGeometry(drawCmd);

        return firstVertex;
    }

    void DebugRenderPass::Bind(const Ref<CommandBuffer>& cmd)
    {
        m_Pipeline->Bind(cmd);
        auto* geometry = m_GeometrySlots.UpdateCurrent([this](SGeometryBuffer& slot)
        {
            if (m_Vertices.empty()) return true;
            if (!slot.Buffer || m_Vertices.size() > slot.Capacity)
            {
                const size_t capacity = GrowBufferCapacity(slot.Capacity, m_Vertices.size());
                auto buffer = DynamicVertexBuffer::Create(m_GraphicsContext, capacity * sizeof(SVertex));
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
            slot.Buffer->UpdateData(m_Vertices.data(), m_Vertices.size() * sizeof(SVertex));
            return true;
        });
        m_CanRender = geometry && geometry->Buffer;
        if (m_CanRender) geometry->Buffer->Bind(cmd);
    }

    void DebugRenderPass::Render(
        const Ref<CommandBuffer>& cmd,
        const uint32_t firstInstance,
        const uint32_t instanceCount
    )
    {
        // Non-instanced LineList draw: reinterpret the generic firstInstance/instanceCount
        // range as firstVertex/vertexCount, matching what AppendRange produced above.
        if (!m_CanRender) return;
        cmd->Draw(instanceCount, 1, firstInstance, 0);
    }

    bool DebugRenderPass::HasData() const
    {
        return !m_Vertices.empty();
    }

    void DebugRenderPass::Clear()
    {
        m_Vertices.clear();
    }

    uint32_t DebugRenderPass::GetInstanceCount() const
    {
        return (uint32_t)m_Vertices.size();
    }

    EDrawCommandType DebugRenderPass::GetHandleType() const
    {
        return EDrawCommandType::DebugRect;
    }

    void DebugRenderPass::InitRenderPass(const ShaderLoader* shaderLoader)
    {
        const BufferLayout bufferLayout({
            {
                {
                    { EDataType::Float2,  "Position" },
                    { EDataType::Float4,  "Color"    },
                }
            }
        });

        m_Shader = shaderLoader->LoadShader("./Shaders/", "Debug");

        PipelineBuilder builder;
        builder.SetShader(m_Shader);
        builder.SetInputTopology(EPrimitiveTopology::LineList);
        builder.SetPolygonMode(EPolygonMode::Line);
        builder.SetCullMode(ECullMode::None, EFrontFace::CounterClockwise);
        builder.DisableBlending();
        builder.DisableDepthTest();
        builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        builder.SetBufferLayout(bufferLayout);
        m_Pipeline = builder.Build(m_GraphicsContext);

        m_BufferLayout = bufferLayout;
    }

    void DebugRenderPass::BindShaderParameters() const
    {
        m_Shader->BindConstantBuffer("cbPerFrame", m_PerFrameConstantBuffer);
    }

    void DebugRenderPass::BuildDebugRectGeometry(const SDrawCommand& cmd)
    {
        // cmd.Geometry arrives in logical points, same as every other pass - QuadRenderPass
        // and TextRenderPass both scale Position/Size by m_DPIScale before building vertex
        // data (see QuadRenderPass.cpp:152-153, TextRenderPass.cpp:198-199); this pass has to
        // match, or its rects land at half the intended screen position on a 2x display.
        const auto position = cmd.Geometry.Position * m_DPIScale;
        const auto size = cmd.Geometry.Size * m_DPIScale;

        const auto topLeft = position;
        const auto topRight = (position + glm::vec2(size.x, 0));
        const auto bottomLeft = (position + glm::vec2(0, size.y));
        const auto bottomRight = (position + size);

        m_Vertices.push_back({ topLeft, cmd.Color });
        m_Vertices.push_back({ topRight, cmd.Color });

        m_Vertices.push_back({ topRight, cmd.Color });
        m_Vertices.push_back({ bottomRight, cmd.Color });

        m_Vertices.push_back({ bottomRight, cmd.Color });
        m_Vertices.push_back({ bottomLeft, cmd.Color });

        m_Vertices.push_back({ bottomLeft, cmd.Color });
        m_Vertices.push_back({ topLeft, cmd.Color });
    }
}
