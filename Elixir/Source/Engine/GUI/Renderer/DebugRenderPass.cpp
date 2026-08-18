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
    ) : m_DPIScale(dpiScale), m_PerFrameConstantBuffer(perFrameCB), m_GraphicsContext(context)
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
        if (!m_Vertices.empty())
            m_VertexBuffer->UpdateData(
                m_Vertices.data(),
                m_Vertices.size() * sizeof(SVertex)
            );
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
        m_VertexBuffer->Bind(cmd);
    }

    void DebugRenderPass::Render(
        const Ref<CommandBuffer>& cmd,
        const uint32_t firstInstance,
        const uint32_t instanceCount
    )
    {
        // Non-instanced LineList draw: reinterpret the generic firstInstance/instanceCount
        // range as firstVertex/vertexCount, matching what AppendRange produced above.
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
                    { EDataType::Vec2,  "Position" },
                    { EDataType::Vec4,  "Color"    },
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

        constexpr auto vertexCount = MAX_LINES * 2;
        m_VertexBuffer = DynamicVertexBuffer::Create(m_GraphicsContext, vertexCount * sizeof(SVertex));
        m_VertexBuffer->SetLayout(bufferLayout);
    }

    void DebugRenderPass::BindShaderParameters() const
    {
        m_Shader->BindConstantBuffer("cbPerFrame", m_PerFrameConstantBuffer);
    }

    void DebugRenderPass::BuildDebugRectGeometry(const SDrawCommand& cmd)
    {
        const auto topLeft = cmd.Geometry.Position;
        const auto topRight = (cmd.Geometry.Position + glm::vec2(cmd.Geometry.Size.x, 0));
        const auto bottomLeft = (cmd.Geometry.Position + glm::vec2(0, cmd.Geometry.Size.y));
        const auto bottomRight = (cmd.Geometry.Position + cmd.Geometry.Size);

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