#include "epch.h"
#include "StaticMeshRenderer.h"

#include <Engine/Mesh/GeometryPool.h>
#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Materials/MaterialInstance.h>
#include <Engine/Materials/MaterialSystem.h>

namespace Elixir
{
    namespace
    {
        struct SSurfaceFrameData
        {
            glm::mat4 View{ 1.0f };
            glm::mat4 Proj{ 1.0f };
            glm::mat4 ViewProj{ 1.0f };
            glm::vec3 CameraPos{};
            float Time = 0.0f;
            float EnvIntensity = 0.0f;
            float EnvMaxLod = 0.0f;
            uint32_t SceneColorIndex = UINT32_MAX;
            float ScreenWidth = 1.0f;
            float ScreenHeight = 1.0f;
            glm::vec4 LightDirection{};
            glm::vec4 LightColor{};
        };

        struct SSurfacePushConstants
        {
            glm::mat4 Model{ 1.0f };
            uint32_t MaterialIndex = 0;
        };
    }

    StaticMeshRenderer::StaticMeshRenderer(
        const GraphicsContext* context,
        MaterialSystem& materialSystem,
        const GeometryPool& geometryPool
    ) : m_MaterialSystem(materialSystem),
        m_GeometryPool(geometryPool),
        m_Context(context)
    {
        EE_CORE_ASSERT(m_Context, "StaticMeshRenderer requires a graphics context")

        constexpr SSurfaceFrameData frameData;
        m_FrameBuffer = UniformBuffer::Create(m_Context, sizeof(frameData), &frameData);
    }

    void StaticMeshRenderer::BeginFrame(
        const Camera& camera,
        const SStaticMeshLighting& lighting
    )
    {
        const auto extent = m_Context->GetRenderTarget()->GetExtent();

        const auto lightDirection = glm::normalize(lighting.DirectionalLightDirection);

        const SSurfaceFrameData frameData{
            .View = camera.GetViewMatrix(),
            .Proj = camera.GetProjectionMatrix(),
            .ViewProj = camera.GetViewProjectionMatrix(),
            .CameraPos = camera.GetPosition(),
            .EnvIntensity = lighting.Environment.Intensity,
            .EnvMaxLod = lighting.Environment.MaxLod,
            .ScreenWidth = (float)extent.Width,
            .ScreenHeight = (float)extent.Height,
            .LightDirection = glm::vec4(lightDirection, 0.0f),
            .LightColor = glm::vec4(
                lighting.DirectionalLightColor,
                lighting.DirectionalLightIntensity
            ),
        };
        m_FrameBuffer->UpdateData(&frameData, sizeof(frameData));

        m_Scene = {};
        m_GeometryIndex = m_Scene.AddGeometry({
            .Pipeline = {
                .VertexLayoutKey = reinterpret_cast<uintptr_t>(&StaticMesh::GetVertexLayout()),
                .VertexLayout = &StaticMesh::GetVertexLayout(),
            },
            .ConstantBuffers = {{
                .Name = "cbFrame",
                .Buffer = m_FrameBuffer,
            }},
            .Textures = {
                {
                    .Name = "environmentTexture",
                    .Texture = lighting.Environment.Environment,
                },
                {
                    .Name = "irradianceTexture",
                    .Texture = lighting.Environment.Irradiance,
                },
                {
                    .Name = "prefilteredTexture",
                    .Texture = lighting.Environment.Prefiltered,
                }
            },
            .Samplers = {{
                .Name = "environmentSampler",
                .Sampler = lighting.Environment.Sampler,
            }},
            .VertexBuffers = {{
                .Buffer = m_GeometryPool.GetVertexBuffer().get(),
                .Binding = 0,
            }},
            .IndexBuffer = m_GeometryPool.GetIndexBuffer().get(),
        });
    }

    void StaticMeshRenderer::Render(const Ref<StaticMesh>& mesh)
    {
        if (!mesh) return;

        const auto geometry = mesh->GetGeometry();
        if (!geometry) return;

        const auto& materials = mesh->GetMaterials();

        for (const auto& section : mesh->GetSections())
        {
            if (section.MaterialIndex >= materials.size()) continue;

            const auto instance = GetDefaultInstance(materials[section.MaterialIndex]);
            if (!instance) continue;

            m_Scene.Add({
                .Pass = EMaterialPass::Surface,
                .Material = instance,
                .GeometryIndex = m_GeometryIndex,
                .PushConstants = SMaterialPushConstants::Create(
                    SSurfacePushConstants{},
                    offsetof(SSurfacePushConstants, MaterialIndex)
                ),
                .IndexedDraw = SIndexedDrawCommand{
                    .IndexCount = section.IndexCount,
                    .FirstIndex = geometry->IndexOffset + section.FirstIndex,
                    .VertexOffset = geometry->VertexOffset + section.VertexOffset,
                },
            });
        }
    }

    void StaticMeshRenderer::EndFrame()
    {
        m_MaterialSystem.Submit(std::move(m_Scene));
    }

    Ref<MaterialInstance> StaticMeshRenderer::GetDefaultInstance(const Ref<Material>& material)
    {
        if (!material) return nullptr;

        const auto existing = m_DefaultInstances.find(material.get());
        if (existing != m_DefaultInstances.end())
        {
            return existing->second;
        }

        const auto instance = material->CreateInstance();
        m_DefaultInstances.emplace(material.get(), instance);
        return instance;
    }
}
