#include "epch.h"
#include "StaticMeshRenderer.h"

#include <Engine/Mesh/GeometryPool.h>
#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/CommandBuffer.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Graphics/Pipeline/PipelineBuilder.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir
{
    namespace
    {
        struct SStaticMeshFrameData
        {
            glm::mat4 ViewProjection{ 1.0f };
        };

        glm::vec3 HsvToRgb(const float hue)
        {
            const glm::vec3 ramp = glm::clamp(
                glm::abs(glm::fract(glm::vec3(hue) + glm::vec3(0.0f, 2.0f / 3.0f, 1.0f / 3.0f)) *
                    6.0f - 3.0f) - 1.0f,
                0.0f,
                1.0f
            );
            return glm::mix(glm::vec3(0.32f), ramp, 0.82f);
        }
    }

    StaticMeshRenderer::StaticMeshRenderer(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const GeometryPool& geometryPool
    ) : m_Context(context),
        m_GeometryPool(geometryPool)
    {
        EE_CORE_ASSERT(m_Context, "StaticMeshRenderer requires a graphics context")
        EE_CORE_ASSERT(shaderLoader, "StaticMeshRenderer requires a shader loader")

        m_Shader = shaderLoader->LoadShader("./Shaders/", "StaticMesh");
        EE_CORE_ASSERT(m_Shader, "StaticMesh visualization shader could not be loaded")

        PipelineBuilder builder;
        builder.SetShader(m_Shader);
        builder.SetInputTopology(EPrimitiveTopology::TriangleList);
        builder.SetPolygonMode(EPolygonMode::Fill);
        builder.DisableBlending();
        builder.DisableDepthTest();
        builder.SetColorAttachmentFormat(EImageFormat::R8G8B8A8_SRGB);
        builder.SetBufferLayout(StaticMesh::GetVertexLayout());
        m_Pipeline = builder.Build(m_Context);

        const SStaticMeshFrameData frameData;
        m_FrameBuffer = UniformBuffer::Create(m_Context, sizeof(frameData), &frameData);
        m_Shader->BindConstantBuffer("cbFrame", m_FrameBuffer);
    }

    void StaticMeshRenderer::BeginFrame(const Camera& camera)
    {
        const SStaticMeshFrameData frameData{ .ViewProjection = camera.GetViewProjectionMatrix() };
        m_FrameBuffer->UpdateData(&frameData, sizeof(frameData));

        const Extent2D extent = m_Context->GetRenderTarget()->GetExtent();
        const SRenderingInfo renderingInfo{
            .ColorAttachment = m_Context->GetRenderTarget(),
            .RenderArea = extent,
        };

        m_CommandBuffer = m_Context->GetSecondaryCommandBuffer();
        m_CommandBuffer->Begin(renderingInfo);
        m_CommandBuffer->BeginRendering(renderingInfo);

        m_CommandBuffer->SetViewports({{
            .Width = static_cast<float>(extent.Width),
            .Height = static_cast<float>(extent.Height),
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
        }});

        m_CommandBuffer->SetScissors({{
            .Offset = { 0, 0 },
            .Extent = extent,
        }});

        m_Pipeline->Bind(m_CommandBuffer);
    }

    void StaticMeshRenderer::Render(const Ref<StaticMesh>& mesh)
    {
        if (!mesh) return;

        glm::vec4 color = GetColor(*mesh);
        m_Shader->SetPushConstant(m_CommandBuffer, "pc", &color, sizeof(color));

        const auto geometry = mesh->GetGeometry();
        if (!geometry) return;

        std::array<const DynamicVertexBuffer*, 1> vertexBuffers = {
            m_GeometryPool.GetVertexBuffer().get()
        };

        m_CommandBuffer->BindVertexBuffers(vertexBuffers);
        m_CommandBuffer->BindIndexBuffer(m_GeometryPool.GetIndexBuffer().get());

        for (const auto& section : mesh->GetSections())
        {
            m_CommandBuffer->DrawIndexed(
                section.IndexCount,
                1,
                geometry->IndexOffset + section.FirstIndex,
                geometry->VertexOffset + section.VertexOffset
            );
        }
    }

    void StaticMeshRenderer::EndFrame()
    {
        m_CommandBuffer->EndRendering();
        m_CommandBuffer->End();
        m_Context->EnqueueSecondaryCommandBuffer(m_CommandBuffer);
    }

    glm::vec4 StaticMeshRenderer::GetColor(const StaticMesh& mesh)
    {
        if (const auto existing = m_Colors.find(&mesh); existing != m_Colors.end())
            return existing->second;

        const uint64_t identity = std::hash<const StaticMesh*>{}(&mesh);
        const float hue = static_cast<float>(identity % 360u) / 360.0f;
        const glm::vec4 color(HsvToRgb(hue), 1.0f);
        m_Colors.emplace(&mesh, color);
        return color;
    }
}
