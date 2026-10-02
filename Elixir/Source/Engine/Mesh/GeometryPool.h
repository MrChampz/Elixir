#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Mesh/GeometryAllocation.h>
#include <Engine/Mesh/StaticMesh.h>

namespace Elixir
{
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
    };

    /**
     * @brief Owns shared GPU buffers used by static meshes.
     *
     * Geometry remains valid until all submitted draws using it complete.
     */
    class ELIXIR_API GeometryPool final
    {
        friend class GeometryAllocation;

    public:
        /**
         * @brief Create shared GPU buffers for static mesh geometry.
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

        /** @brief Get geometry metadata for a valid handle. */
        const SGeometry* Get(SHandle<SGeometry> handle) const;

        /** @brief Get the shared vertex buffer. */
        const Ref<DynamicVertexBuffer>& GetVertexBuffer() const { return m_VertexBuffer; }

        /** @brief Get the shared index buffer. */
        const Ref<DynamicIndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }

    private:
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

        Ref<DynamicVertexBuffer> m_VertexBuffer;
        Ref<DynamicIndexBuffer> m_IndexBuffer;
        std::vector<SGeometry> m_Geometries;
        std::vector<uint32_t> m_Generations;

        const GraphicsContext& m_GraphicsContext;
    };
}
