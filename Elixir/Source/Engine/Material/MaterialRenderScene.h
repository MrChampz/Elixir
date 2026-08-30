#pragma once

#include <Engine/Material/MaterialRenderer.h>

namespace Elixir
{
    /**
     * @brief Stores push constants for one material draw.
     *
     * The structure keeps the raw constant data and can replace the material index
     * before the draw is recorded.
     */
    struct SMaterialPushConstants
    {
        /** Maximum number of bytes available for push constants. */
        static constexpr uint32_t CAPACITY = 128;

        /** Indicates that no material-index replacement is required. */
        static constexpr uint32_t NO_OFFSET = UINT32_MAX;

        /** Raw push-constant data. */
        std::array<std::byte, CAPACITY> Data{};

        /** Number of valid bytes in @ref Data. */
        uint32_t Size = 0;

        /** Byte offset of the material index in @ref Data, or @ref NO_OFFSET. */
        uint32_t MaterialIndexOffset = NO_OFFSET;

        /**
         * @brief Creates push constants from a value.
         *
         * @tparam T Type of the source value.
         * @param value Value copied into the push-constant storage.
         * @param materialIndexOffset Byte offset of the material index, if present.
         * @return Push constants containing a copy of @p value.
         *
         * @pre `sizeof(T)` must not exceed @ref CAPACITY.
         */
        template <typename T>
        static SMaterialPushConstants Create(
            const T& value,
            const uint32_t materialIndexOffset = NO_OFFSET
        )
        {
            EE_CORE_ASSERT(
                sizeof(T) <= CAPACITY,
                "Material push constants exceed the scene storage capacity."
            )

            SMaterialPushConstants pc{};
            Memory::Memcpy(pc.Data.data(), &value, sizeof(T));
            pc.Size = sizeof(T);
            pc.MaterialIndexOffset = materialIndexOffset;

            return pc;
        }

        /**
         * @brief Returns the push constants with the material index applied.
         *
         * @param materialIndex Index of the material in the current frame table.
         * @return A copy of @ref Data with the material index written when required.
         */
        std::array<std::byte, CAPACITY> Resolve(uint32_t materialIndex) const;
    };

    /**
     * @brief Associates a vertex buffer with a pipeline binding.
     */
    struct SMaterialVertexBufferBinding
    {
        /** Source vertex buffer. */
        const Buffer* Buffer = nullptr;

        /** Vertex-input binding index. */
        uint32_t Binding = 0;
    };

    /**
     * @brief Describes one indexed range of a draw call.
     */
    struct SMaterialDrawCommand
    {
        /** Number of vertices to draw. */
        uint32_t VertexCount = 0;

        /** Number of instances to draw. */
        uint32_t InstanceCount = 1;

        /** First vertex in the source buffer. */
        uint32_t FirstVertex = 0;

        /** First instance in the source buffer. */
        uint32_t FirstInstance = 0;
    };

    /**
     * @brief Stores shared resources for render items with compatible geometry.
     */
    struct SMaterialRenderGeometry
    {
        /** Pipeline configuration for the geometry. */
        SMaterialPipelineRequest Pipeline;

        /** Constant buffers required by the material pass. */
        std::vector<SMaterialConstantBufferBinding> ConstantBuffers;

        /** Storage buffers required by the material pass. */
        std::vector<SMaterialStorageBufferBinding> StorageBuffers;

        /** Vertex buffers required by the draw. */
        std::vector<SMaterialVertexBufferBinding> VertexBuffers;
    };

    /**
     * @brief Describes one material draw recorded for the current frame.
     */
    struct SMaterialRenderItem
    {
        /** Material pass used to render the item. */
        EMaterialPass Pass = EMaterialPass::ParticleSprite;

        /** Resolved material data used by the pass. */
        Ref<const MaterialRenderProxy> Material;

        /** Index of the geometry used by this item. */
        uint32_t GeometryIndex = UINT32_MAX;

        /** Push constants applied before the draw. */
        SMaterialPushConstants PushConstants;

        /** Draw range for the item. */
        SMaterialDrawCommand Draw;
    };

    /**
     * @brief Collects geometry and material draws for one frame.
     *
     * The scene becomes immutable after publication and is consumed while the
     * frame command buffer is recorded.
     */
    class ELIXIR_API MaterialRenderScene final
    {
    public:
        /**
         * @brief Adds reusable geometry to the scene.
         *
         * @param geometry Geometry resources to store.
         * @return Index that identifies the stored geometry.
         *
         * @pre `geometry.Pipeline.VertexLayout` is not null.
         */
        uint32_t AddGeometry(SMaterialRenderGeometry geometry);

        /**
         * @brief Adds a material draw to the scene.
         *
         * @param item Draw description to store.
         *
         * @pre `item.GeometryIndex` identifies geometry added to this scene.
         */
        void Add(SMaterialRenderItem item);

        /**
         * @brief Finds geometry by index.
         *
         * @param index Geometry index.
         * @return The geometry, or null when @p index is invalid.
         */
        const SMaterialRenderGeometry* FindGeometry(uint32_t index) const;

        /**
         * @brief Returns the material draws in insertion order.
         * @return Read-only view of the frame-local draw items.
         */
        std::span<const SMaterialRenderItem> GetItems() const { return m_Items; }

    private:
        std::vector<SMaterialRenderGeometry> m_Geometries;
        std::vector<SMaterialRenderItem> m_Items;
    };
}
