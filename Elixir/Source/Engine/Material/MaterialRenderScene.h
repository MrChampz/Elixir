#pragma once

#include <Engine/Material/MaterialRenderer.h>

namespace Elixir
{
    struct SMaterialPushConstants
    {
        static constexpr uint32_t CAPACITY = 128;
        static constexpr uint32_t NO_OFFSET = UINT32_MAX;

        std::array<std::byte, CAPACITY> Data{};
        uint32_t Size = 0;
        uint32_t MaterialIndexOffset = NO_OFFSET;

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

        std::array<std::byte, CAPACITY> Resolve(uint32_t materialIndex) const;
    };

    struct SMaterialVertexBufferBinding
    {
        const Buffer* Buffer = nullptr;
        uint32_t Binding = 0;
    };

    struct SMaterialDrawCommand
    {
        uint32_t VertexCount = 0;
        uint32_t InstanceCount = 1;
        uint32_t FirstVertex = 0;
        uint32_t FirstInstance = 0;
    };

    // Geometry resources are shared by all items that use a particle-state
    // layout and render primitive in the current frame.
    struct SMaterialRenderGeometry
    {
        SMaterialPipelineRequest Pipeline;
        std::vector<SMaterialConstantBufferBinding> ConstantBuffers;
        std::vector<SMaterialStorageBufferBinding> StorageBuffers;
        std::vector<SMaterialVertexBufferBinding> VertexBuffers;
    };

    // A frame-local material item. Geometry producers may temporarily expose
    // an additional texture while their legacy data is migrated into MaterialRenderProxy.
    struct SMaterialRenderItem
    {
        EMaterialPass Pass = EMaterialPass::ParticleSprite;
        Ref<const MaterialRenderProxy> Material;
        std::string_view DebugName;
        uint32_t GeometryIndex = UINT32_MAX;
        SMaterialPushConstants PushConstants;
        SMaterialDrawCommand Draw;
    };

    // Immutable after frame publication. MaterialSystem consumes this object
    // synchronously while recording the frame command buffer.
    class ELIXIR_API MaterialRenderScene final
    {
    public:
        uint32_t AddGeometry(SMaterialRenderGeometry geometry);
        void Add(SMaterialRenderItem item);

        const SMaterialRenderGeometry* FindGeometry(uint32_t index) const;

        std::span<const SMaterialRenderItem> GetItems() const { return m_Items; }

    private:
        std::vector<SMaterialRenderGeometry> m_Geometries;
        std::vector<SMaterialRenderItem> m_Items;
    };
}