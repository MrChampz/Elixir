#include "epch.h"
#include "SkeletalMeshRenderer.h"

#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Materials/MaterialInstance.h>
#include <Engine/Materials/MaterialSystem.h>

#include <Engine/Mesh/StaticMesh.h>

namespace Elixir
{
    using namespace Materials;

    namespace
    {
        struct alignas(16) SSurfaceFrameData
        {
            glm::mat4 View{ 1.0f };
            glm::mat4 Proj{ 1.0f };
            glm::mat4 ViewProj{ 1.0f };
            glm::vec3 CameraPos{};
            float Time = 0.0f;
            float EnvIntensity = 0.0f;
            float EnvMaxLod = 0.0f;
            uint32_t SceneColorIndex = UINT32_MAX;
            uint32_t DebugView = 0;
            float ScreenWidth = 1.0f;
            float ScreenHeight = 1.0f;
            glm::vec2 Padding{};
            glm::vec4 LightDirection{};
            glm::vec4 LightColor{};
        };

        struct SSurfacePushConstants
        {
            glm::mat4 Model{ 1.0f };
            uint32_t MaterialIndex = 0;
        };
    }

    SkeletalMeshRenderer::SkeletalMeshRenderer(
        const GraphicsContext& context,
        MaterialSystem& materialSystem,
        const ShaderLoader& shaderLoader,
        const SSkeletalMeshRendererConfig& config
    ) : m_Config(config),
        m_Sorter(CreateScope<SectionSorter>()),
        m_SkinningPass(context, shaderLoader),
        m_Frames(context),
        m_FrameBuffers(context),
        m_GeometryPool(context, config.SourceGeometry),
        m_MaterialSystem(materialSystem),
        m_Context(context)
    {
        if (m_GeometryPool.GetVertexStride() != sizeof(SSkeletalMeshVertex) ||
            config.FrameGeometry.VertexStride != sizeof(SStaticMeshVertex) ||
            !HasAnyFlags(config.SourceGeometry.VertexUsage, EBufferUsage::StorageBuffer) ||
            !HasAnyFlags(config.FrameGeometry.VertexUsage, EBufferUsage::StorageBuffer) ||
            config.BoneCapacity == 0)
        {
            EE_CORE_ERROR("SkeletalMeshRenderer requires storage-enabled skeletal source and Surface output buffers, and bone capacity.")
            m_Config.BoneCapacity = 0;
            return;
        }

        constexpr SSurfaceFrameData frameData;
        m_FrameBuffers.ForEach([&](Ref<UniformBuffer>& buffer)
        {
            buffer = UniformBuffer::Create(&context, sizeof(frameData), &frameData);
        });
    }

    SkeletalMeshRenderer::~SkeletalMeshRenderer()
    {
        const auto result = m_Context.RunRenderTaskAndWait([&]
        {
            m_Context.WaitForSubmittedFrames();
        });

        if (!result)
        {
            // The render queue can already be drained during application shutdown.
            m_Context.WaitDeviceIdle();
        }
    }

    void SkeletalMeshRenderer::BeginFrame(const Camera& camera, const SMeshLighting& lighting)
    {
        if (m_Recording)
        {
            EE_CORE_ERROR("SkeletalMeshRenderer already has an open frame.")
            return;
        }

        if (m_Config.BoneCapacity == 0 ||
            !m_SkinningPass.IsValid() ||
            !m_GeometryPool.IsValid())
            return;

        auto& frame = m_Frames.GetCurrent();

        if (!frame.Geometry)
            frame.Geometry = CreateScope<GeometryPool>(m_Context, m_Config.FrameGeometry);

        if (!frame.Geometry->IsValid()) return;

        frame.Reset();
        m_Batch.Reset();

        std::erase_if(
            m_SourceGeometry,
            [](const auto& entry) { return entry.second.Mesh.expired(); }
        );

        m_Recording = true;
        const auto extent = m_Context.GetRenderTarget()->GetExtent();
        m_View = camera.GetViewMatrix();

        const auto lightDirection = glm::normalize(lighting.DirectionalLightDirection);

        const SSurfaceFrameData frameData{
            .View = m_View,
            .Proj = camera.GetProjectionMatrix(),
            .ViewProj = camera.GetViewProjectionMatrix(),
            .CameraPos = camera.GetPosition(),
            .EnvIntensity = lighting.Environment.Intensity,
            .EnvMaxLod = lighting.Environment.MaxLod,
            .DebugView = (uint32_t)lighting.DebugView,
            .ScreenWidth = (float)extent.Width,
            .ScreenHeight = (float)extent.Height,
            .LightDirection = glm::vec4(lightDirection, 0.0f),
            .LightColor = glm::vec4(
                lighting.DirectionalLightColor,
                lighting.DirectionalLightIntensity
            ),
        };
        const auto& frameBuffer = m_FrameBuffers.GetCurrent();
        frameBuffer->UpdateData(&frameData, sizeof(frameData));

        m_Scene = {};
        SRenderGeometry geometry{
            .Pipeline = {
                .VertexLayoutKey = reinterpret_cast<uintptr_t>(&GetMeshVertexLayout()),
                .VertexLayout = &GetMeshVertexLayout(),
            },
            .ConstantBuffers = {{
                .Name = "cbFrame",
                .Buffer = frameBuffer,
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
                .Buffer = frame.Geometry->GetVertexBuffer().get(),
                .Binding = 0
            }},
            .IndexBuffer = m_GeometryPool.GetIndexBuffer().get(),
        };

        frame.GeometryIndex = m_Scene.AddGeometry(std::move(geometry));
    }

    bool SkeletalMeshRenderer::Render(
        const SkeletalMeshInstance& instance,
        const glm::mat4& transform
    )
    {
        return Render(instance, instance.GetReferenceModelPose(), transform);
    }

    bool SkeletalMeshRenderer::Render(
        const SkeletalMeshInstance& instance,
        const SModelPose& modelPose,
        const glm::mat4& transform
    )
    {
        if (!m_Recording || !instance.GetMesh())
        {
            EE_CORE_ERROR("SkeletalMeshRenderer requires an open frame and a mesh instance.")
            return false;
        }

        const auto& mesh = instance.GetMesh();

        const auto* source = GetGeometry(mesh);
        if (!source) return false;

        const auto& data = mesh->GetData();
        auto& frame = m_Frames.GetCurrent();
        auto allocation = frame.Geometry->Allocate(source->VertexCount);

        const auto* output = allocation.Get();
        if (!output) return false;

        const auto appended = m_Batch.Append(
            *mesh,
            modelPose,
            source->VertexOffset,
            output->VertexOffset,
            m_Config.BoneCapacity
        );

        if (!appended) return false;

        m_Sorter->SetContext({ .Model = transform, .View = m_View });

        for (const auto& section : data.Sections)
        {
            const auto material = GetDefaultInstance(data.Materials[section.MaterialIndex]);
            if (!material) continue;

            const auto depth = m_Sorter->CalculateDepth(section);

            m_Scene.Add({
                .Pass = EMaterialPass::Surface,
                .Material = material,
                .SortDepth = depth,
                .GeometryIndex = frame.GeometryIndex,
                .PushConstants = SMaterialPushConstants::Create(
                    SSurfacePushConstants{ .Model = transform },
                    offsetof(SSurfacePushConstants, MaterialIndex)
                ),
                .IndexedDraw = SIndexedDrawCommand{
                    .IndexCount = section.IndexCount,
                    .FirstIndex = source->IndexOffset + section.FirstIndex,
                    .VertexOffset = output->VertexOffset + section.VertexOffset,
                },
            });
        }

        frame.Allocations.push_back(std::move(allocation));
        frame.Meshes.push_back(mesh);

        return true;
    }

    void SkeletalMeshRenderer::EndFrame()
    {
        if (!m_Recording)
        {
            EE_CORE_ERROR("SkeletalMeshRenderer has no open frame to submit.")
            return;
        }

        m_Recording = false;

        if (!m_Batch.GetJobs().empty())
        {
            const auto cmd = m_Context.GetSecondaryCommandBuffer();
            if (!cmd) return;

            cmd->Begin();
            const auto success = m_SkinningPass.Record(
                cmd,
                m_Batch,
                m_GeometryPool.GetVertexBuffer(),
                m_Frames.GetCurrent().Geometry->GetVertexBuffer()
            );

            cmd->End();
            if (!success) return;

            m_Context.EnqueueSecondaryCommandBuffer(cmd);
        }

        m_MaterialSystem.Submit(std::move(m_Scene));
    }

    void SkeletalMeshRenderer::SetSorter(Scope<Sorter> sorter)
    {
        if (!sorter) return;
        m_Sorter = std::move(sorter);
    }

    const SGeometry* SkeletalMeshRenderer::GetGeometry(const Ref<const SkeletalMesh>& mesh)
    {
        const auto found = m_SourceGeometry.find(mesh.get());
        if (found != m_SourceGeometry.end() && !found->second.Mesh.expired())
            return found->second.Allocation.Get();

        auto allocation = m_GeometryPool.Upload(mesh->GetData());
        if (!allocation.IsValid()) return nullptr;

        auto [entry, inserted] = m_SourceGeometry.insert_or_assign(
            mesh.get(),
            SSourceGeometry{ mesh, std::move(allocation) }
        );

        return entry->second.Allocation.Get();
    }

    Ref<MaterialInstance> SkeletalMeshRenderer::GetDefaultInstance(
        const Ref<Material>& material
    )
    {
        if (!material) return nullptr;

        const auto existing = m_DefaultInstances.find(material.get());
        if (existing != m_DefaultInstances.end())
            return existing->second;

        const auto instance = material->CreateInstance();
        m_DefaultInstances.emplace(material.get(), instance);
        return instance;
    }
}
