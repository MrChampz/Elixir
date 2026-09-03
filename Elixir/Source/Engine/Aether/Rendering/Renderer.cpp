#include "epch.h"
#include "Renderer.h"

namespace Elixir::Aether::Rendering
{
    using namespace Core;
    using namespace Materials::Rendering;

    namespace
    {
        struct SMeshVertex
        {
            glm::vec3 Position;
            glm::vec3 Normal;
        };

        struct SSpritePushConstants
        {
            glm::mat4 WorldTransform{ 1.0f };
            uint32_t MaterialIndex = UINT32_MAX;
        };

        struct SMeshPushConstants
        {
            glm::mat4 WorldTransform{ 1.0f };
            uint32_t MaterialIndex = UINT32_MAX;
        };

        struct SRibbonPushConstants
        {
            glm::mat4 WorldTransform{ 1.0f };
            uint32_t EmitterIndex = 0;
            uint32_t ParticleBaseOffset = 0;
            uint32_t MaterialIndex = UINT32_MAX;
        };
    }

    Renderer::Renderer(const GraphicsContext* context)
      : m_GraphicsContext(context)
    {
        EE_CORE_ASSERT(context, "Aether Renderer requires a graphics context.")
        EE_CORE_INFO("Initializing Aether Renderer.")

        CreateCoreV1GraphicsLayout();
        CreateMeshVertexBuffer();
        InitPerFrameData();
    }

    MaterialRenderScene Renderer::BuildMaterialRenderScene(
        const RenderFrame& frame,
        const Camera& camera
    )
    {
        m_LastMetrics = { .SubmissionSerial = frame.GetSubmissionSerial() };

        m_FrameData.View = camera.GetViewMatrix();
        m_FrameData.Proj = camera.GetProjectionMatrix();
        m_FrameData.ViewProj = camera.GetViewProjectionMatrix();
        m_FrameData.CameraPos = camera.GetPosition();
        m_FrameData.Time = frame.GetElapsedTimeSeconds();
        m_FrameConstantBuffer->UpdateData(&m_FrameData, sizeof(m_FrameData));

        auto scene = BuildScene(frame);
        m_LastMetrics.SubmittedRenderItemCount = scene.GetItems().size();
        return scene;
    }

    void Renderer::CreateCoreV1GraphicsLayout()
    {
        SParticleGraphicsLayout layout{
            .Key = EParticleStateLayout::CoreV1,
        };

        layout.SpriteVertexLayout = {{
            {
                {
                    { EDataType::Vec4,  "PositionSize"    },
                    { EDataType::Vec4,  "VelocityAge"     },
                    { EDataType::Vec4,  "Transform"       },
                    { EDataType::Vec4,  "TangentRibbonId" },
                    { EDataType::Vec4,  "Color"           },
                    { EDataType::Vec4,  "Metadata"        }
                },
                EInputRate::Instance
            }
        }};

        layout.MeshVertexLayout = {{
            {
                {
                    { EDataType::Vec3,  "Position" },
                    { EDataType::Vec3,  "Normal"   },
                },
                EInputRate::Vertex
            },
            {
                {
                    { EDataType::Vec4,  "PositionSize"    },
                    { EDataType::Vec4,  "VelocityAge"     },
                    { EDataType::Vec4,  "Transform"       },
                    { EDataType::Vec4,  "TangentRibbonId" },
                    { EDataType::Vec4,  "Color"           },
                    { EDataType::Vec4,  "Metadata"        }
                },
                EInputRate::Instance
            }
        }};

        m_GraphicsLayouts.push_back(std::move(layout));
    }

    void Renderer::CreateMeshVertexBuffer()
    {
        static constexpr std::array<SMeshVertex, 36> vertices = {{
            {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},

            {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},

            {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},

            {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},

            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},

            {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
            {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
        }};

        m_MeshVertexCount = (uint32_t)vertices.size();
        m_MeshVertexBuffer = VertexBuffer::Create(
            m_GraphicsContext,
            sizeof(vertices),
            vertices.data()
        );

        const auto* layout = FindGraphicsLayout(EParticleStateLayout::CoreV1);
        EE_CORE_ASSERT(layout, "Aether CoreV1 graphics layout is missing.")
        if (!layout) return;

        m_MeshVertexBuffer->SetLayout(layout->MeshVertexLayout);
    }

    void Renderer::InitPerFrameData()
    {
        m_FrameConstantBuffer = UniformBuffer::Create(
            m_GraphicsContext,
            sizeof(m_FrameData),
            &m_FrameData
        );
    }

    const Renderer::SParticleGraphicsLayout* Renderer::FindGraphicsLayout(
        const EParticleStateLayout key
    ) const
    {
        for (const auto& layout : m_GraphicsLayouts)
            if (layout.Key == key) return &layout;
        return nullptr;
    }

    const SParticleStateRenderResource* Renderer::FindRenderResource(
        const RenderFrame& frame,
        const EParticleStateLayout key
    )
    {
        for (const auto& resource : frame.GetResources())
            if (resource.Layout == key) return &resource;

        return nullptr;
    }

    MaterialRenderScene Renderer::BuildScene(const RenderFrame& frame) const
    {
        MaterialRenderScene scene;

        static const BufferLayout ribbonVertexLayout;

        struct SGeometryIndices
        {
            uint32_t Sprite = UINT32_MAX;
            uint32_t Ribbon = UINT32_MAX;
            uint32_t Mesh   = UINT32_MAX;
        };

        std::unordered_map<uint32_t, SGeometryIndices> geometries;

        const auto getGeometry = [this, &frame, &scene, &geometries](
            const EParticleStateLayout key
        ) -> std::optional<SGeometryIndices>
        {
            const auto cacheKey = (uint32_t)key;
            if (const auto found = geometries.find(cacheKey); found != geometries.end())
                return found->second;

            const auto* layout = FindGraphicsLayout(key);
            EE_CORE_ASSERT(layout, "Aether graphics layout is missing.")

            const auto* resource = FindRenderResource(frame, key);
            EE_CORE_ASSERT(resource, "Aether render resource is missing.");

            if (!layout || !resource || !resource->ParticleStateBuffer)
                return std::nullopt;

            const std::array constantBuffers{
                SConstantBufferBinding{
                    .Name = "cbFrame",
                    .Buffer = m_FrameConstantBuffer,
                },
            };

            const std::array ribbonStorageBuffers{
                SStorageBufferBinding{
                    .Name = "particles",
                    .Buffer = MaterialStorageBuffer{ resource->ParticleStateBuffer },
                },
                SStorageBufferBinding{
                    .Name = "emitters",
                    .Buffer = MaterialStorageBuffer{ frame.GetEmitterBuffer() },
                },
            };

            const SGeometryIndices indices{
                .Sprite = scene.AddGeometry({
                    .Pipeline = {
                        .VertexLayoutKey = cacheKey,
                        .VertexLayout = &layout->SpriteVertexLayout,
                    },
                    .ConstantBuffers = { constantBuffers.begin(), constantBuffers.end() },
                    .VertexBuffers = {
                        { .Buffer = resource->ParticleStateBuffer.get(), .Binding = 0 }
                    },
                }),
                .Ribbon = scene.AddGeometry({
                    .Pipeline = {
                        .VertexLayoutKey = cacheKey,
                        .VertexLayout = &ribbonVertexLayout,
                    },
                    .ConstantBuffers = { constantBuffers.begin(), constantBuffers.end() },
                    .StorageBuffers = { ribbonStorageBuffers.begin(), ribbonStorageBuffers.end() },
                }),
                .Mesh = scene.AddGeometry({
                    .Pipeline = {
                        .VertexLayoutKey = cacheKey,
                        .VertexLayout = &layout->MeshVertexLayout,
                    },
                    .ConstantBuffers = { constantBuffers.begin(), constantBuffers.end() },
                    .VertexBuffers = {
                        { .Buffer = m_MeshVertexBuffer.get(), .Binding = 0 },
                        { .Buffer = resource->ParticleStateBuffer.get(), .Binding = 1 },
                    },
                }),
            };

            geometries.emplace(cacheKey, indices);
            return indices;
        };

        for (const auto& item : frame.GetItems())
        {
            const auto geometry = getGeometry(item.ParticleStateLayout);
            if (!geometry) continue;

            switch (item.RenderMode)
            {
                case EParticleRenderMode::Sprite:
                {
                    const SSpritePushConstants constants{
                        .WorldTransform = item.WorldTransform,
                    };

                    scene.Add({
                        .Pass = EMaterialPass::ParticleSprite,
                        .Material = item.Material,
                        .GeometryIndex = geometry->Sprite,
                        .PushConstants = SMaterialPushConstants::Create(
                            constants,
                            offsetof(SSpritePushConstants, MaterialIndex)
                        ),
                        .Draw = {
                            .VertexCount = 6,
                            .InstanceCount = item.ParticleCount,
                            .FirstInstance = item.Allocation.Particles.Offset +
                                item.LocalParticleOffset,
                        },
                    });
                    break;
                }

                case EParticleRenderMode::Ribbon:
                {
                    const SRibbonPushConstants constants{
                        .WorldTransform = item.WorldTransform,
                        .EmitterIndex = item.Allocation.Emitters.Offset + item.EmitterIndex,
                        .ParticleBaseOffset = item.Allocation.Particles.Offset,
                    };

                    scene.Add({
                        .Pass = EMaterialPass::ParticleRibbon,
                        .Material = item.Material,
                        .GeometryIndex = geometry->Ribbon,
                        .PushConstants = SMaterialPushConstants::Create(
                            constants,
                            offsetof(SRibbonPushConstants, MaterialIndex)
                        ),
                        .Draw = { .VertexCount = item.ParticleCount * 6 },
                    });
                    break;
                }

                case EParticleRenderMode::Mesh:
                {
                    const SMeshPushConstants constants{
                        .WorldTransform = item.WorldTransform,
                    };

                    scene.Add({
                        .Pass = EMaterialPass::ParticleMesh,
                        .Material = item.Material,
                        .GeometryIndex = geometry->Mesh,
                        .PushConstants = SMaterialPushConstants::Create(
                            constants,
                            offsetof(SMeshPushConstants, MaterialIndex)
                        ),
                        .Draw = {
                            .VertexCount = m_MeshVertexCount,
                            .InstanceCount = item.ParticleCount,
                            .FirstInstance = item.Allocation.Particles.Offset +
                                item.LocalParticleOffset,
                        },
                    });
                    break;
                }
            }
        }

        return scene;
    }
}
