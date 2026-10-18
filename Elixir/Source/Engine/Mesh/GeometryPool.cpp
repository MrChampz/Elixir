#include "epch.h"
#include "GeometryPool.h"

#include <Engine/Mesh/StaticMesh.h>
#include <Engine/Mesh/SkeletalMesh.h>
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
        m_VertexStride = config.VertexStride;

        if (config.VertexCapacity == 0 ||
            config.IndexCapacity == 0 ||
            config.VertexStride == 0 ||
            config.VertexStride % sizeof(uint32_t) != 0 ||
            uint64_t(config.VertexCapacity) * config.VertexStride > UINT32_MAX)
        {
            EE_CORE_ERROR("GeometryPool requires nonzero capacities and a word-aligned stride within the 32-bit byte range.")
            return;
        }

        m_VertexBuffer = StorageBuffer::Create(
            &context,
            size_t(config.VertexCapacity) * config.VertexStride
        );

        m_IndexBuffer = DynamicIndexBuffer::Create(
            &context,
            size_t(config.IndexCapacity) * sizeof(uint32_t)
        );

        if (!IsValid())
            EE_CORE_ERROR("GeometryPool failed to create its GPU buffers.")
    }

    bool GeometryPool::IsValid() const
    {
        return m_VertexBuffer && m_VertexBuffer->IsValid() &&
            m_IndexBuffer && m_IndexBuffer->IsValid();
    }

    GeometryAllocation GeometryPool::Upload(const SStaticMeshData& data)
    {
        if (data.Vertices.size() > UINT32_MAX) return {};
        return UploadData(
            data.Vertices.data(),
            (uint32_t)data.Vertices.size(),
            sizeof(SStaticMeshVertex),
            data.Indices
        );
    }

    GeometryAllocation GeometryPool::Upload(const SSkeletalMeshData& data)
    {
        if (data.Vertices.size() > UINT32_MAX) return {};
        return UploadData(
            data.Vertices.data(),
            (uint32_t)data.Vertices.size(),
            sizeof(SSkeletalMeshVertex),
            data.Indices
        );
    }

    GeometryAllocation GeometryPool::UploadData(
        const void* vertices,
        const uint32_t vertexCount,
        const uint32_t stride,
        const std::span<const uint32_t> indices
    )
    {
        if (!vertices ||
            vertexCount == 0 ||
            indices.empty() ||
            indices.size() > UINT32_MAX ||
            stride != m_VertexStride
        )
        {
            EE_CORE_ERROR("GeometryPool upload requires nonempty geometry matching its vertex stride.")
            return {};
        }

        auto allocation = Allocate(vertexCount, (uint32_t)indices.size());
        const auto* geometry = allocation.Get();
        if (!geometry) return {};

        const auto staging = StagingBuffer::Create(
            &m_GraphicsContext,
            size_t(vertexCount) * stride,
            vertices
        );

        if (!staging || !staging->IsValid())
        {
            EE_CORE_ERROR("GeometryPool failed to create its upload buffer.")
            return {};
        }

        const auto cmd = m_GraphicsContext.GetUploadCommandBuffer();
        if (!cmd) return {};

        cmd->Begin();
        m_VertexBuffer->Barrier(
            cmd,
            EPipelineStage::Transfer,
            EPipelineAccess::TransferWrite
        );

        SBufferCopy copy{
            .SrcOffset = 0,
            .DstOffset = size_t(geometry->VertexOffset) * stride,
            .Size = size_t(vertexCount) * stride
        };
        staging->Copy(cmd, m_VertexBuffer, std::span{ &copy, 1 });

        m_VertexBuffer->Barrier(
            cmd,
            EPipelineStage::ComputeShader | EPipelineStage::VertexInput,
            EPipelineAccess::ShaderRead | EPipelineAccess::VertexAttributeRead
        );

        cmd->Flush();

        m_IndexBuffer->UpdateData(
            indices.data(),
            indices.size_bytes(),
            size_t(geometry->IndexOffset) * sizeof(uint32_t)
        );

        return allocation;
    }

    GeometryAllocation GeometryPool::Allocate(
        const uint32_t vertexCount,
        const uint32_t indexCount
    )
    {
        if (!IsValid() || vertexCount == 0)
        {
            EE_CORE_ERROR("GeometryPool allocation requires valid buffers and a nonzero vertex count.")
            return {};
        }

        const auto vertexOffset = AllocateRange(
            m_State->FreeVertexRanges,
            m_State->NextVertexOffset,
            m_State->VertexCapacity,
            vertexCount
        );

        if (!vertexOffset)
        {
            EE_CORE_ERROR("GeometryPool has no free vertex range.")
            return {};
        }

        const auto indexOffset = indexCount == 0
            ? std::optional<uint32_t>(0)
            : AllocateRange(
                m_State->FreeIndexRanges,
                m_State->NextIndexOffset,
                m_State->IndexCapacity,
                indexCount
            );

        if (!indexOffset)
        {
            FreeRange(m_State->FreeVertexRanges, { *vertexOffset, vertexCount });
            EE_CORE_ERROR("GeometryPool has no free index range.")
            return {};
        }

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

    void GeometryPool::Reset()
    {
        m_State->NextVertexOffset = 0;
        m_State->NextIndexOffset = 0;
        m_State->FreeVertexRanges.clear();
        m_State->FreeIndexRanges.clear();
        m_State->FreeSlots.clear();

        for (uint32_t i = 0; i < m_State->Slots.size(); ++i)
        {
            auto& slot = m_State->Slots[i];
            slot.Allocated = false;
            slot.Geometry = {};
            ++slot.Generation;

            m_State->FreeSlots.push_back(i);
        }
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
        if (range.Count == 0) return;

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
