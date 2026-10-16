#pragma once

#include <Engine/Mesh/SkeletalMeshInstance.h>
#include <Engine/Mesh/MeshLighting.h>
#include <Engine/Graphics/FrameSlotState.h>
#include <Engine/Materials/Rendering/MaterialRenderScene.h>

namespace Elixir
{
    namespace Materials { class MaterialSystem; }
    using namespace Materials;

    class Camera;

    /** @brief Draws skeletal mesh instances through Surface materials using CPU skinning. */
    class ELIXIR_API SkeletalMeshRenderer final
    {
    public:
        /** @brief Create a renderer; the graphics context and material system must outlive it. */
        SkeletalMeshRenderer(
            const GraphicsContext& context,
            MaterialSystem& materialSystem
        );

        /**
         * @brief Begin a frame and advance each mesh playback once by deltaSeconds.
         * @pre The frame slot fence completed, and the previous frame was closed.
         */
        void BeginFrame(
            const Camera& camera,
            const SMeshLighting& lighting,
            float deltaSeconds = 0.0f
        );

        /**
         * @brief Start a sequence for a mesh submitted to this renderer; return false on failure.
         * Repeated submissions of the same mesh share playback within this renderer.
         */
        bool Play(
            const Ref<const SkeletalMesh>& mesh,
            Ref<const AnimationSequence> sequence,
            bool looping = true
        );

        /** @brief Restore the reference pose of a mesh submitted to this renderer. */
        void Stop(const Ref<const SkeletalMesh>& mesh);

        /**
         * @brief Record all sections of one complete skeletal mesh at its current pose.
         * @pre BeginFrame was called and EndFrame has not been called yet.
         */
        bool Render(const Ref<const SkeletalMesh>& mesh);

        /** @brief Submit draws and close the frame. */
        void EndFrame();

    private:
        // Skin and record a renderer-owned mesh instance.
        bool RenderPose(const SkeletalMeshInstance& meshInstance);

        // Reuse one default instance per material.
        Ref<MaterialInstance> GetDefaultInstance(const Ref<Material>& material);

        struct SDrawBuffers
        {
            Ref<const SkeletalMesh> Mesh;
            Ref<DynamicVertexBuffer> Vertices;
            Ref<DynamicIndexBuffer> Indices;
        };

        MaterialSystem& m_MaterialSystem;

        FrameSlotState<std::vector<SDrawBuffers>> m_DrawBuffers;

        std::unordered_map<const SkeletalMesh*, SkeletalMeshInstance> m_Playbacks;

        size_t m_DrawIndex = 0;
        bool m_Recording = false;
        Rendering::MaterialRenderScene m_Scene;
        Rendering::SRenderGeometry m_FrameGeometry;
        std::unordered_map<const Material*, Ref<MaterialInstance>> m_DefaultInstances;
        glm::mat4 m_View{ 1.0f };

        const GraphicsContext& m_Context;
        FrameSlotState<Ref<UniformBuffer>> m_FrameBuffers;
    };
}
