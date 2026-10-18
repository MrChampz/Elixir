#pragma once

#include <Engine/Mesh/SkeletalMeshInstance.h>
#include <Engine/Mesh/GeometryPool.h>
#include <Engine/Mesh/MeshLighting.h>
#include <Engine/Mesh/SkinningPass.h>
#include <Engine/Materials/Rendering/MaterialRenderScene.h>

namespace Elixir
{
    namespace Materials { class MaterialSystem; }
    using namespace Materials;

    class Camera;

    /** @brief Sets per-frame geometry and bone limits for batched skeletal rendering. */
    struct SSkeletalMeshRendererConfig
    {
        /** Shared source capacity for skeletal vertices and their original indices. */
        SGeometryPoolConfig SourceGeometry{
            1'000'000,
            3'000'000,
            sizeof(SSkeletalMeshVertex)
        };

        /** Shared output capacity per frame slot, using the Surface vertex layout. */
        SGeometryPoolConfig FrameGeometry{
            1'000'000,
            3'000'000,
            sizeof(SStaticMeshVertex)
        };

        /** Maximum number of mesh-palette matrices uploaded in one frame. */
        uint32_t BoneCapacity = 65'536;
    };

    /** @brief Collects skeletal draws and deforms their shared geometry with batched compute. */
    class ELIXIR_API SkeletalMeshRenderer final
    {
    public:
        /**
         * @brief Create a renderer that owns its source and per-frame geometry pools.
         * @pre context and materialSystem outlive this renderer.
         */
        SkeletalMeshRenderer(
            const GraphicsContext& context,
            MaterialSystem& materialSystem,
            const ShaderLoader& shaderLoader,
            const SSkeletalMeshRendererConfig& config = {}
        );

        /** @brief Wait for submitted draws before releasing the shared frame buffers. */
        ~SkeletalMeshRenderer();

        /**
         * @brief Begin collecting draws and advance renderer-owned asset playbacks.
         * @pre The current frame slot fence completed, and the previous frame was closed.
         */
        void BeginFrame(
            const Camera& camera,
            const SMeshLighting& lighting
        );

        /**
         * @brief Record an independent instance and a model-to-world transform.
         * The pose is captured during this call; advance playback before submitting it.
         */
        bool Render(
            const SkeletalMeshInstance& instance,
            const glm::mat4& transform = glm::mat4(1)
        );

        /** @brief Dispatch collected skinning work and submit surface draws. */
        void EndFrame();

    private:
        struct SSourceGeometry
        {
            WeakRef<const SkeletalMesh> Mesh;
            GeometryAllocation Allocation;
        };

        struct SFrameState
        {
            Scope<GeometryPool> Geometry;
            std::vector<GeometryAllocation> Allocations;
            std::vector<Ref<const SkeletalMesh>> Meshes;
            uint32_t GeometryIndex = UINT32_MAX;

            void Reset()
            {
                Geometry->Reset();
                Allocations.clear();
                Meshes.clear();
                GeometryIndex = UINT32_MAX;
            }
        };

        // Upload a source asset once and retain its allocation while the asset exists.
        const SGeometry* GetGeometry(const Ref<const SkeletalMesh>& mesh);

        // Reuse one default instance per material.
        Ref<MaterialInstance> GetDefaultInstance(const Ref<Material>& material);

        SSkeletalMeshRendererConfig m_Config;

        SkinningBatch m_Batch;
        SkinningPass m_SkinningPass;

        FrameSlotState<SFrameState> m_Frames;
        FrameSlotState<Ref<UniformBuffer>> m_FrameBuffers;

        GeometryPool m_GeometryPool;
        std::unordered_map<const SkeletalMesh*, SSourceGeometry> m_SourceGeometry;

        MaterialSystem& m_MaterialSystem;
        Rendering::MaterialRenderScene m_Scene;
        std::unordered_map<const Material*, Ref<MaterialInstance>> m_DefaultInstances;

        glm::mat4 m_View{1};

        bool m_Recording = false;

        const GraphicsContext& m_Context;
    };
}
