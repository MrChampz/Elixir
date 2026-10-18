#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Mesh/GeometryAllocation.h>
#include <Engine/Mesh/Mesh.h>
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
         * @brief Upload CPU mesh data into the shared GPU buffers.
         * @return An allocation that owns the uploaded geometry, or an empty allocation
         * when the data is invalid or the pool lacks capacity.
         */
        GeometryAllocation Upload(const SStaticMeshData& data);

        /**
         * @brief Upload skeletal source vertices and indices; the pool stride must match them.
         * @return An allocation that owns the uploaded geometry, or an empty allocation
         * when the data is invalid or the pool lacks capacity.
         */
        GeometryAllocation Upload(const SSkeletalMeshData& data);

        /** @brief Reserve geometry without uploading vertices; zero indices are allowed. */
        GeometryAllocation Allocate(uint32_t vertexCount, uint32_t indexCount = 0);

        /**
         * @brief Invalidate all allocations and reuse their storage.
         * @pre All GPU work using this pool completed, including work in other frame slots.
         */
        void Reset();

        /** @brief Check whether both GPU buffers were created. */
        bool IsValid() const;

        /** @brief Get geometry metadata for a valid handle. */
        const SGeometry* Get(SHandle<SGeometry> handle) const;

        /** @brief Get the shared vertex buffer. */
        const Ref<VertexBuffer>& GetVertexBuffer() const { return m_VertexBuffer; }

        /** @brief Get the shared index buffer. */
        const Ref<DynamicIndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }

        /** @brief Get the configured byte stride of vertices. */
        uint32_t GetVertexStride() const { return m_VertexStride; }

    private:
        // Upload geometry bytes after validating the configured vertex stride.
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

        /** @brief Defers a geometry release until submitted draws no longer use it. */
        void Free(SHandle<SGeometry> handle);

        /** @brief Releases a geometry range when its handle is valid. */
        static void FreeCompleted(SGeometryPoolState& state, SHandle<SGeometry> handle);

        /** @brief Check whether this allocation still identifies live pool geometry. */
        bool IsValid(const SHandle<SGeometry>& handle) const;

        /** @brief Check whether a handle identifies live geometry in a pool state. */
        static bool IsHandleValid(
            const SGeometryPoolState& state,
            const SHandle<SGeometry>& handle
        );

        /**
         * @brief Allocate one contiguous range in a pool buffer.
         * @return First element offset, or std::nullopt when capacity is exhausted.
         */
        static std::optional<uint32_t> AllocateRange(
            std::vector<SGeometryPoolRange>& freeRanges,
            uint32_t& nextOffset,
            uint32_t capacity,
            uint32_t count
        );

        /** @brief Release a range and merge it with adjacent free ranges. */
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
