#include "epch.h"
#include "SkeletalMeshRenderer.h"

#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Materials/MaterialInstance.h>
#include <Engine/Materials/MaterialSystem.h>

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
        MaterialSystem& materialSystem
    ) : m_MaterialSystem(materialSystem),
        m_DrawBuffers(context),
        m_Context(context),
        m_FrameBuffers(context)
    {
        constexpr SSurfaceFrameData frameData;
        m_FrameBuffers.ForEach([&](Ref<UniformBuffer>& buffer)
        {
            buffer = UniformBuffer::Create(&context, sizeof(frameData), &frameData);
        });
    }

    void SkeletalMeshRenderer::BeginFrame(
        const Camera& camera,
        const SMeshLighting& lighting,
        const float deltaSeconds
    )
    {
        if (m_Recording)
        {
            EE_CORE_ERROR("SkeletalMeshRenderer already has an open frame.")
            return;
        }

        for (auto& playback : m_Playbacks | std::views::values)
            playback.Update(deltaSeconds);

        m_Recording = true;
        m_DrawIndex = 0;

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
        m_FrameGeometry = {
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
        };
    }

    bool SkeletalMeshRenderer::Play(
        const Ref<const SkeletalMesh>& mesh,
        Ref<const AnimationSequence> sequence,
        const bool looping
    )
    {
        if (!mesh || !sequence || !mesh->GetSkeleton()->IsCompatible(*sequence->GetSkeleton()))
        {
            EE_CORE_ERROR("SkeletalMeshRenderer requires a mesh and a compatible animation.")
            return false;
        }

        auto [it, inserted] = m_Playbacks.try_emplace(mesh.get(), mesh);

        return it->second.Play(std::move(sequence), looping);
    }

    void SkeletalMeshRenderer::Stop(const Ref<const SkeletalMesh>& mesh)
    {
        if (mesh)
            m_Playbacks.erase(mesh.get());
    }

    bool SkeletalMeshRenderer::Render(const Ref<const SkeletalMesh>& mesh)
    {
        if (!mesh) return false;

        const auto found = m_Playbacks.find(mesh.get());
        if (found != m_Playbacks.end())
            return RenderPose(found->second);

        return RenderPose(SkeletalMeshInstance(mesh));
    }

    bool SkeletalMeshRenderer::RenderPose(const SkeletalMeshInstance& meshInstance)
    {
        if (!m_Recording || !meshInstance.GetMesh())
        {
            EE_CORE_ERROR("SkeletalMeshRenderer requires an open frame and a mesh instance.")
            return false;
        }

        std::vector<SStaticMeshVertex> vertices;
        const auto& mesh = meshInstance.GetMesh();

        if (!mesh->SkinVertices(meshInstance.GetModelPose(), vertices))
            return false;

        const auto& data = mesh->GetData();

        auto& buffers = m_DrawBuffers.GetCurrent();
        if (buffers.size() <= m_DrawIndex)
            buffers.emplace_back();

        auto& draw = buffers[m_DrawIndex];
        if (draw.Mesh != mesh || !draw.Vertices || !draw.Indices)
        {
            draw.Mesh = mesh;
            draw.Vertices = DynamicVertexBuffer::Create(
                &m_Context,
                vertices.size() * sizeof(SStaticMeshVertex)
            );
            draw.Indices = IndexBuffer::Create(
                &m_Context,
                data.Indices.size() * sizeof(uint32_t),
                data.Indices.data()
            );
        }

        if (!draw.Vertices ||
            !draw.Indices ||
            !draw.Vertices->IsValid() ||
            !draw.Indices->IsValid())
        {
            EE_CORE_ERROR("SkeletalMeshRenderer failed to allocate draw buffers.")
            return false;
        }

        draw.Vertices->UpdateData(
            vertices.data(),
            vertices.size() * sizeof(SStaticMeshVertex)
        );

        auto geometry = m_FrameGeometry;
        geometry.VertexBuffers = {{
            .Buffer = draw.Vertices.get(),
            .Binding = 0
        }};
        geometry.IndexBuffer = draw.Indices.get();

        const auto geometryIndex = m_Scene.AddGeometry(std::move(geometry));
        ++m_DrawIndex;

        for (const auto& section : data.Sections)
        {
            auto instance = GetDefaultInstance(data.Materials[section.MaterialIndex]);
            if (!instance) continue;

            glm::vec3 min(std::numeric_limits<float>::max());
            glm::vec3 max(std::numeric_limits<float>::lowest());

            for (size_t i = section.FirstIndex; i < size_t(section.FirstIndex) + section.IndexCount; ++i)
            {
                const auto position = vertices[size_t(section.VertexOffset) + data.Indices[i]].Position;
                min = glm::min(min, position);
                max = glm::max(max, position);
            }

            const auto center = (min + max) * 0.5f;

            m_Scene.Add({
                .Pass = EMaterialPass::Surface,
                .Material = instance,
                .SortDepth = -(m_View * glm::vec4(center, 1.0f)).z,
                .GeometryIndex = geometryIndex,
                .PushConstants = SMaterialPushConstants::Create(
                    SSurfacePushConstants{},
                    offsetof(SSurfacePushConstants, MaterialIndex)
                ),
                .IndexedDraw = SIndexedDrawCommand{
                    .IndexCount = section.IndexCount,
                    .FirstIndex = section.FirstIndex,
                    .VertexOffset = section.VertexOffset,
                },
            });
        }
        return true;
    }

    void SkeletalMeshRenderer::EndFrame()
    {
        if (!m_Recording)
        {
            EE_CORE_ERROR("SkeletalMeshRenderer has no open frame to submit.")
            return;
        }

        m_MaterialSystem.Submit(std::move(m_Scene));
        m_DrawBuffers.GetCurrent().resize(m_DrawIndex);
        m_Recording = false;
    }

    Ref<MaterialInstance> SkeletalMeshRenderer::GetDefaultInstance(const Ref<Material>& material)
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
