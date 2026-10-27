#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Mesh/GeometryAllocation.h>
#include <Engine/Mesh/StaticMesh.h>

namespace Elixir
{
    struct SStaticMeshData;
    struct SSkeletalMeshData;

    /** @brief Identifies a contiguous geometry range in GeometryPool buffers. */
    struct SGeometry
    {
        uint32_t VertexOffset = 0;
        uint32_t IndexOffset = 0;
        uint32_t VertexCount = 0;
        uint32_t IndexCount = 0;
    };

    /** @brief Defines the fixed capacity of one geometry pool. */
    struct SGeometryPoolConfig
    {
        uint32_t VertexCapacity = 4'000'000;
        uint32_t IndexCapacity = 12'000'000;

        /** Byte stride of source or output vertices; the default is the Surface layout. */
        uint32_t VertexStride = sizeof(SStaticMeshVertex);

        /**
         * Vertex buffer roles, including VertexBuffer. The pool adds transfer source and
         * destination usages.
         */
        EBufferUsage VertexUsage = EBufferUsage::VertexBuffer;
    };

    /**
     * @brief Owns shared GPU geometry buffers for upload, compute, and drawing.
     *
     * Geometry remains valid until all submitted draws using it complete.
     */
    class ELIXIR_API GeometryPool final
    {
        friend class GeometryAllocation;

    public:
        /**
         * @brief Create shared GPU buffers with the configured vertex stride.
         * @param context Graphics context that owns the buffers.
         * @param config Capacity of the shared buffers.
         */
        explicit GeometryPool(
            const GraphicsContext& context,
            SGeometryPoolConfig config = {}
        );

        /**
         * @brief Upload static source geometry matching the pool vertex stride.
         * @param data Source vertices and section-local triangle indices.
         * @return An owning allocation, or an empty allocation for invalid data or upload
         * failure.
         */
        GeometryAllocation Upload(const SStaticMeshData& data);

        /**
         * @brief Upload skeletal source geometry matching the pool vertex stride.
         * @param data Source vertices and section-local triangle indices.
         * @return An owning allocation, or an empty allocation for invalid data or upload
         * failure.
         */
        GeometryAllocation Upload(const SSkeletalMeshData& data);

        /**
         * @brief Reserve geometry without uploading vertices.
         * @param vertexCount Nonzero number of vertices to reserve.
         * @param indexCount Number of indices to reserve; zero reserves no index range.
         * @return An allocation, or an empty allocation for invalid input or insufficient
         * capacity.
         */
        GeometryAllocation Allocate(uint32_t vertexCount, uint32_t indexCount = 0);

        /**
         * @brief Invalidate all allocations and reuse their storage.
         * @pre All GPU work using this pool completed, including work in other frame slots.
         */
        void Reset();

        /** @brief Check whether both GPU buffers were created. */
        bool IsValid() const;

        /**
         * @brief Get geometry metadata for a live handle.
         * @param handle Allocation handle to resolve.
         * @return Geometry metadata, or null for an invalid or expired handle.
         */
        const SGeometry* Get(SHandle<SGeometry> handle) const;

        /** @brief Get the shared vertex buffer. */
        const Ref<VertexBuffer>& GetVertexBuffer() const { return m_VertexBuffer; }

        /** @brief Get the shared index buffer. */
        const Ref<DynamicIndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }

        /** @brief Get the configured byte stride of vertices. */
        uint32_t GetVertexStride() const { return m_VertexStride; }

    private:
        /**
         * @brief Upload geometry bytes after checking the configured vertex stride.
         * @param vertices First source vertex byte; must not be null.
         * @param vertexCount Nonzero number of source vertices.
         * @param stride Byte stride of each source vertex; must match the pool stride.
         * @param indices Nonempty section-local triangle indices.
         * @return An owning allocation, or an empty allocation if validation or upload fails.
         */
        GeometryAllocation UploadData(
            const void* vertices,
            uint32_t vertexCount,
            uint32_t stride,
            std::span<const uint32_t> indices
        );

        struct SGeometryPoolRange
        {
            uint32_t Offset = 0;
            uint32_t Count = 0;
        };

        struct SGeometryPoolSlot
        {
            SGeometry Geometry;
            uint32_t Generation = 0;
            bool Allocated = false;
        };

        /** @brief Holds allocation metadata that can outlive the pool. */
        struct SGeometryPoolState
        {
            uint32_t VertexCapacity = 0;
            uint32_t IndexCapacity = 0;
            uint32_t NextVertexOffset = 0;
            uint32_t NextIndexOffset = 0;

            std::vector<SGeometryPoolRange> FreeVertexRanges;
            std::vector<SGeometryPoolRange> FreeIndexRanges;
            std::vector<SGeometryPoolSlot> Slots;
            std::vector<uint32_t> FreeSlots;
        };

        /**
         * @brief Defer a geometry release until submitted draws finish.
         * @param handle Allocation handle to release.
         */
        void Free(SHandle<SGeometry> handle);

        /**
         * @brief Release a geometry range when its handle is valid.
         * @param state Pool state that owns the allocation.
         * @param handle Allocation handle to release.
         */
        static void FreeCompleted(SGeometryPoolState& state, SHandle<SGeometry> handle);

        /**
         * @brief Check whether an allocation still identifies live pool geometry.
         * @param handle Allocation handle to check.
         */
        bool IsValid(const SHandle<SGeometry>& handle) const;

        /**
         * @brief Check whether a handle identifies live geometry in a pool state.
         * @param state Pool state that owns the allocation.
         * @param handle Allocation handle to check.
         */
        static bool IsHandleValid(
            const SGeometryPoolState& state,
            const SHandle<SGeometry>& handle
        );

        /**
         * @brief Allocate a contiguous range in a pool buffer.
         * @param freeRanges Reusable ranges updated when a range is allocated.
         * @param nextOffset End of the occupied buffer prefix, advanced when needed.
         * @param capacity Maximum number of buffer elements.
         * @param count Number of contiguous elements to reserve.
         * @return First element offset, or no value when capacity is exhausted.
         */
        static std::optional<uint32_t> AllocateRange(
            std::vector<SGeometryPoolRange>& freeRanges,
            uint32_t& nextOffset,
            uint32_t capacity,
            uint32_t count
        );

        /**
         * @brief Release a range and merge it with adjacent free ranges.
         * @param freeRanges Reusable ranges to update.
         * @param range Element range to release.
         */
        static void FreeRange(
            std::vector<SGeometryPoolRange>& freeRanges,
            SGeometryPoolRange range
        );

        Ref<SGeometryPoolState> m_State;

        Ref<VertexBuffer> m_VertexBuffer;
        uint32_t m_VertexStride = 0;
        Ref<DynamicIndexBuffer> m_IndexBuffer;
        std::vector<SGeometry> m_Geometries;
        std::vector<uint32_t> m_Generations;

        const GraphicsContext& m_GraphicsContext;
    };
}
