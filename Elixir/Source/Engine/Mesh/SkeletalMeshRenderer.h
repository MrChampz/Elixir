#pragma once

#include <Engine/Mesh/SkeletalMeshInstance.h>
#include <Engine/Mesh/GeometryPool.h>
#include <Engine/Mesh/MeshLighting.h>
#include <Engine/Mesh/SkinningPass.h>
#include <Engine/Mesh/Sorter.h>
#include <Engine/Materials/Rendering/MaterialRenderScene.h>

namespace Elixir
{
    namespace Materials { class MaterialSystem; }
    using namespace Materials;

    class Camera;

    /** Defines persistent source capacity and per-frame output and bone limits. */
    struct SSkeletalMeshRendererConfig
    {
        /**
         * Persistent capacity shared by all source assets. VertexStride must match
         * sizeof(SSkeletalMeshVertex). Vertices and indices are uploaded once per asset.
         * VertexUsage must include VertexBuffer and StorageBuffer.
         */
        SGeometryPoolConfig SourceGeometry{
            1'000'000,
            3'000'000,
            sizeof(SSkeletalMeshVertex),
            EBufferUsage::VertexBuffer | EBufferUsage::StorageBuffer
        };

        /**
         * Output capacity per frame slot, shared by all submitted instances. VertexStride must
         * match sizeof(SStaticMeshVertex). Draws reuse indices from SourceGeometry.
         * VertexUsage must include VertexBuffer and StorageBuffer.
         */
        SGeometryPoolConfig FrameGeometry{
            1'000'000,
            3'000'000,
            sizeof(SStaticMeshVertex),
            EBufferUsage::VertexBuffer | EBufferUsage::StorageBuffer
        };

        /**
         * Nonzero limit on skin matrices summed across all instances submitted in one frame.
         */
        uint32_t BoneCapacity = 65'536;
    };

    /**
     * @brief Deform skeletal mesh instances with batched compute and collect surface draws.
     *
     * Owns a shared source geometry pool and separate output pools for frame slots in flight.
     * Instances supply poses computed on the CPU; this renderer does not advance animation.
     * BeginFrame opens a batch, Render captures instances, and EndFrame records the GPU work.
     */
    class ELIXIR_API SkeletalMeshRenderer final
    {
    public:
        /**
         * @brief Create a renderer that owns its source and per-frame geometry pools.
         * @param context Graphics context used to allocate buffers and record GPU commands.
         * @param materialSystem Receives the collected surface draws at EndFrame.
         * @param shaderLoader Loads the compute shader used by the skinning pass.
         * @param config Source, output, and bone capacities copied into this renderer.
         * @pre The context and material system outlive this renderer.
         */
        SkeletalMeshRenderer(
            const GraphicsContext& context,
            MaterialSystem& materialSystem,
            const ShaderLoader& shaderLoader,
            const SSkeletalMeshRendererConfig& config = {}
        );

        /** @brief Wait for submitted GPU work before releasing owned geometry and buffers. */
        ~SkeletalMeshRenderer();

        /**
         * @brief Begin collecting skeletal instances for the current frame slot.
         *
         * Reuses the slot's completed output allocations, clears the skinning batch, and
         * updates camera and lighting data. Releases source allocations for expired assets.
         * Instance animation must be advanced by the caller before Render.
         *
         * @param camera Supplies view, projection, position, and section sorting depths.
         * @param lighting Surface lighting and environment; light direction must be nonzero.
         * @pre The previous frame was closed and the current slot's GPU work completed.
         */
        void BeginFrame(
            const Camera& camera,
            const SMeshLighting& lighting
        );

        /**
         * @brief Append an instance's current pose and surface draws to the frame.
         *
         * Uploads uncached source geometry, reserves a separate output range, and captures
         * skin matrices in the batch. Each section with a usable material produces one draw.
         * The sorter calculates each section's depth; SectionSorter uses reference-pose bounds
         * by default, rather than deformed triangle depths.
         *
         * @param instance Supplies a mesh and its current skeleton-space model pose.
         * @param transform Model-to-world matrix used by the draw and depth strategy.
         * @return True if the instance was added to the batch. Returns false if the frame is
         * closed, the mesh or pose is invalid, or geometry or bone capacity is insufficient.
         * @pre BeginFrame opened the frame and EndFrame has not closed it.
         */
        bool Render(
            const SkeletalMeshInstance& instance,
            const glm::mat4& transform = glm::mat4(1)
        );

        /**
         * @brief Close the frame, record compute skinning, and submit the surface draws.
         *
         * Enqueues the compute commands before submitting the material scene. The skinning
         * pass makes its output available to vertex input without copying it back to the CPU.
         * If compute recording fails, the collected surface draws are not submitted.
         *
         * @pre BeginFrame opened the frame and EndFrame has not closed it.
         */
        void EndFrame();

        /**
         * @brief Select the depth strategy used by subsequent Render calls.
         * @param sorter Depth sorter; if null is provided, do nothing.
         */
        void SetSorter(Scope<Sorter> sorter);

    private:
        /** Associates a source asset with its persistent GPU allocation. */
        struct SSourceGeometry
        {
            /** Weak reference used to release the allocation when the asset expires. */
            WeakRef<const SkeletalMesh> Mesh;

            /** Owns the source vertices and indices in the renderer's shared pool. */
            GeometryAllocation Allocation;
        };

        /** Retains skinning output and asset lifetimes until one frame slot completes. */
        struct SFrameState
        {
            /** Pool of deformed vertices for this slot; source indices remain shared. */
            Scope<GeometryPool> Geometry;

            /** Keeps each submitted instance's output range reserved for the slot. */
            std::vector<GeometryAllocation> Allocations;

            /** Keeps source assets alive while submitted GPU work may use their geometry. */
            std::vector<Ref<const SkeletalMesh>> Meshes;

            /** Output geometry entry in the material scene, or UINT32_MAX before setup. */
            uint32_t GeometryIndex = UINT32_MAX;

            /**
             * @brief Release slot references and reset its output allocations for reuse.
             * @pre Geometry is valid and all GPU work using this slot completed.
             */
            void Reset()
            {
                Geometry->Reset();
                Allocations.clear();
                Meshes.clear();
                GeometryIndex = UINT32_MAX;
            }
        };

        /**
         * @brief Reuse or upload a source asset's persistent geometry allocation.
         * @param mesh Source asset to find or upload; must not be null.
         * @return Geometry in the shared source pool, or null if upload fails.
         */
        const SGeometry* GetGeometry(const Ref<const SkeletalMesh>& mesh);

        /**
         * @brief Reuse or create the default instance for a material.
         * @param material Material to cache; null is accepted and returns null.
         * @return Cached instance, or null if the material is absent or creation fails.
         */
        Ref<MaterialInstance> GetDefaultInstance(const Ref<Material>& material);

        SSkeletalMeshRendererConfig m_Config;
        Scope<Sorter> m_Sorter;

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
