#include "epch.h"
#include "StaticMeshRenderer.h"

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
        const ShaderLoader* shaderLoader
    ) : m_Context(context)
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

    void StaticMeshRenderer::Render(
        const std::span<const Ref<StaticMesh>> meshes,
        const Camera& camera
    )
    {
        if (meshes.empty()) return;

        const SStaticMeshFrameData frameData{ .ViewProjection = camera.GetViewProjectionMatrix() };
        m_FrameBuffer->UpdateData(&frameData, sizeof(frameData));

        const Extent2D extent = m_Context->GetRenderTarget()->GetExtent();
        const SRenderingInfo renderingInfo{
            .ColorAttachment = m_Context->GetRenderTarget(),
            .RenderArea = extent,
        };

        const auto commandBuffer = m_Context->GetSecondaryCommandBuffer();
        commandBuffer->Begin(renderingInfo);
        commandBuffer->BeginRendering(renderingInfo);
        commandBuffer->SetViewports({{
            .Width = static_cast<float>(extent.Width),
            .Height = static_cast<float>(extent.Height),
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
        }});
        commandBuffer->SetScissors({{
            .Offset = { 0, 0 },
            .Extent = extent,
        }});
        m_Pipeline->Bind(commandBuffer);

        for (const Ref<StaticMesh>& mesh : meshes)
        {
            if (!mesh) continue;

            glm::vec4 color = GetColor(*mesh);
            m_Shader->SetPushConstant(commandBuffer, "pc", &color, sizeof(color));

            for (const SStaticMeshSection& section : mesh->GetSections())
            {
                if (!section.Vertices || !section.Indices || section.IndexCount == 0) continue;

                section.Vertices->Bind(commandBuffer);
                section.Indices->Bind(commandBuffer);
                commandBuffer->DrawIndexed(section.IndexCount);
            }
        }

        commandBuffer->EndRendering();
        commandBuffer->End();
        m_Context->EnqueueSecondaryCommandBuffer(commandBuffer);
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
