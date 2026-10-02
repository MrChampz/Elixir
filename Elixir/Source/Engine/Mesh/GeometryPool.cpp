#include "epch.h"
#include "GeometryPool.h"

#include <Engine/Graphics/GraphicsContext.h>

namespace Elixir
{
    GeometryPool::GeometryPool(
        const GraphicsContext& context,
        const SGeometryPoolConfig config
    ) : m_State(CreateRef<SGeometryPoolState>(SGeometryPoolState{
            .VertexCapacity = config.VertexCapacity,
            .IndexCapacity = config.IndexCapacity,
        })),
        m_GraphicsContext(context)
    {
        EE_CORE_ASSERT(m_State->VertexCapacity > 0, "GeometryPool vertex capacity must be greater than 0.")
        EE_CORE_ASSERT(m_State->IndexCapacity > 0, "GeometryPool index capacity must be greater than 0.")

        m_VertexBuffer = DynamicVertexBuffer::Create(
            &context,
            m_State->VertexCapacity * sizeof(SStaticMeshVertex)
        );

        m_IndexBuffer = DynamicIndexBuffer::Create(
            &context,
            m_State->IndexCapacity * sizeof(uint32_t),
            nullptr,
            EIndexType::UInt32
        );

        EE_CORE_ASSERT(m_VertexBuffer, "GeometryPool failed to create the vertex buffer.")
        EE_CORE_ASSERT(m_IndexBuffer, "GeometryPool failed to create the index buffer.")

        m_VertexBuffer->SetLayout(StaticMesh::GetVertexLayout());
    }

    GeometryAllocation GeometryPool::Upload(const SStaticMeshData& data)
    {
        if (data.Vertices.empty() || data.Indices.empty())
        {
            EE_CORE_ERROR("GeometryPool cannot upload empty geometry.")
            return {};
        }

        if (data.Vertices.size() > std::numeric_limits<uint32_t>::max() ||
            data.Indices.size() > std::numeric_limits<uint32_t>::max())
        {
            EE_CORE_ERROR("GeometryPool geometry exceeds the supported element count.")
            return {};
        }

        const uint32_t vertexCount = (uint32_t)data.Vertices.size();
        const uint32_t indexCount = (uint32_t)data.Indices.size();

        const auto vertexOffset = AllocateRange(
            m_State->FreeVertexRanges,
            m_State->NextVertexOffset,
            m_State->VertexCapacity,
            vertexCount
        );
        if (!vertexOffset)
        {
            EE_CORE_ERROR("GeometryPool has no free vertex range for '{}'.", data.Name)
            return {};
        }

        const auto indexOffset = AllocateRange(
            m_State->FreeIndexRanges,
            m_State->NextIndexOffset,
            m_State->IndexCapacity,
            indexCount
        );
        if (!indexOffset)
        {
            FreeRange(
                m_State->FreeVertexRanges,
                {
                    .Offset = *vertexOffset,
                    .Count = vertexCount,
                }
            );

            EE_CORE_ERROR("GeometryPool has no free index range for '{}'.", data.Name)
            return {};
        }

        m_VertexBuffer->UpdateData(
            data.Vertices.data(),
            data.Vertices.size() * sizeof(SStaticMeshVertex),
            (size_t)*vertexOffset * sizeof(SStaticMeshVertex)
        );
        m_IndexBuffer->UpdateData(
            data.Indices.data(),
            data.Indices.size() * sizeof(uint32_t),
            (size_t)*indexOffset * sizeof(uint32_t)
        );

        uint32_t slotIndex;
        if (!m_State->FreeSlots.empty())
        {
            slotIndex = m_State->FreeSlots.back();
            m_State->FreeSlots.pop_back();
        }
        else
        {
            slotIndex = (uint32_t)m_State->Slots.size();
            m_State->Slots.push_back({});
        }

        SGeometryPoolSlot& slot = m_State->Slots[slotIndex];
        slot.Geometry = {
            .VertexOffset = *vertexOffset,
            .IndexOffset = *indexOffset,
            .VertexCount = vertexCount,
            .IndexCount = indexCount,
        };
        slot.Allocated = true;

        return GeometryAllocation(
            this,
            {
                .Index = slotIndex,
                .Generation = slot.Generation,
            }
        );
    }

    const SGeometry* GeometryPool::Get(const SHandle<SGeometry> handle) const
    {
        if (!handle.IsValid() || handle.Index >= m_State->Slots.size())
            return nullptr;

        const SGeometryPoolSlot& slot = m_State->Slots[handle.Index];
        if (!slot.Allocated || slot.Generation != handle.Generation)
            return nullptr;

        return &slot.Geometry;
    }

    void GeometryPool::Free(const SHandle<SGeometry> handle)
    {
        if (!handle.IsValid() || handle.Index >= m_State->Slots.size())
            return;

        const SGeometryPoolSlot& slot = m_State->Slots[handle.Index];
        if (!slot.Allocated || slot.Generation != handle.Generation)
            return;

        const Ref<SGeometryPoolState> state = m_State;
        m_GraphicsContext.DeferResourceRelease([state, handle]()
        {
            FreeCompleted(*state, handle);
        });
    }

    void GeometryPool::FreeCompleted(
        SGeometryPoolState& state,
        const SHandle<SGeometry> handle
    )
    {
        if (!handle.IsValid() || handle.Index >= state.Slots.size())
            return;

        SGeometryPoolSlot& slot = state.Slots[handle.Index];
        if (!slot.Allocated || slot.Generation != handle.Generation)
            return;

        FreeRange(
            state.FreeVertexRanges,
        {
                .Offset = slot.Geometry.VertexOffset,
                .Count = slot.Geometry.VertexCount,
            }
        );

        FreeRange(
            state.FreeIndexRanges,
        {
                .Offset = slot.Geometry.IndexOffset,
                .Count = slot.Geometry.IndexCount,
            }
        );

        slot.Allocated = false;
        ++slot.Generation;
        slot.Geometry = {};
        state.FreeSlots.push_back(handle.Index);
    }

    bool GeometryPool::IsValid(const SHandle<SGeometry>& handle) const
    {
        return IsHandleValid(*m_State, handle);
    }

    bool GeometryPool::IsHandleValid(
        const SGeometryPoolState& state,
        const SHandle<SGeometry>& handle
    )
    {
        return handle.IsValid() &&
            handle.Index < state.Slots.size() &&
            state.Slots[handle.Index].Allocated &&
            state.Slots[handle.Index].Generation == handle.Generation;
    }

    std::optional<uint32_t> GeometryPool::AllocateRange(
        std::vector<SGeometryPoolRange>& freeRanges,
        uint32_t& nextOffset,
        const uint32_t capacity,
        const uint32_t count
    )
    {
        for (auto it = freeRanges.begin(); it != freeRanges.end(); ++it)
        {
            if (it->Count < count)
                continue;

            const uint32_t offset = it->Offset;
            it->Offset += count;
            it->Count -= count;

            if (it->Count == 0)
                freeRanges.erase(it);

            return offset;
        }

        if (count > capacity - nextOffset)
            return std::nullopt;

        const uint32_t offset = nextOffset;
        nextOffset += count;
        return offset;
    }

    void GeometryPool::FreeRange(
        std::vector<SGeometryPoolRange>& freeRanges,
        const SGeometryPoolRange range
    )
    {
        auto it = std::lower_bound(
            freeRanges.begin(),
            freeRanges.end(),
            range.Offset,
            [](const SGeometryPoolRange& entry, const uint32_t offset)
            {
               return entry.Offset < offset;
            }
        );

        it = freeRanges.insert(it, range);

        if (it != freeRanges.begin())
        {
            const auto previous = std::prev(it);
            if (previous->Offset + previous->Count == it->Offset)
            {
                previous->Count += it->Count;
                it = freeRanges.erase(it);
                it = previous;
            }
        }

        const auto next = std::next(it);
        if (next != freeRanges.end() &&
            it->Offset + it->Count == next->Offset)
        {
            it->Count += next->Count;
            freeRanges.erase(next);
        }
    }
}
