#include "epch.h"
#include "D3D12Buffer.h"

#ifdef EE_PLATFORM_WINDOWS

#include <Graphics/D3D12/D3D12CommandBuffer.h>

#include <type_traits>

namespace Elixir::D3D12
{
    namespace
    {
        using D3D12BufferTypes = std::tuple<
            D3D12BaseBuffer<Buffer>,
            D3D12DynamicBuffer<StagingBuffer>,
            D3D12BaseBuffer<VertexBuffer>,
            D3D12DynamicBuffer<DynamicVertexBuffer>,
            D3D12BaseBuffer<IndexBuffer>,
            D3D12DynamicBuffer<DynamicIndexBuffer>,
            D3D12BaseBuffer<StorageBuffer>,
            D3D12DynamicBuffer<DynamicStorageBuffer>,
            D3D12DynamicBuffer<UniformBuffer>
        >;

        template <size_t I = 0>
        ID3D12Resource* TryToGetD3D12BufferHandle(const Buffer* buffer)
        {
            if constexpr (I < std::tuple_size_v<D3D12BufferTypes>)
            {
                using BufferType = std::tuple_element_t<I, D3D12BufferTypes>;
                if (const auto buff = dynamic_cast<const BufferType*>(buffer))
                    return buff->GetD3D12Resource();
                return TryToGetD3D12BufferHandle<I + 1>(buffer);
            }
            else
            {
                EE_CORE_ASSERT(false, "Unsupported D3D12 buffer type!")
                return nullptr;
            }
        }

        D3D12_HEAP_TYPE GetHeapType(const SAllocationInfo& info)
        {
            if ((info.RequiredFlags & EMemoryProperty::HostVisible) ||
                (info.PreferredFlags & EMemoryProperty::HostVisible))
            {
                return D3D12_HEAP_TYPE_UPLOAD;
            }

            return D3D12_HEAP_TYPE_DEFAULT;
        }

        uint64_t GetResourceSize(const SBufferCreateInfo& info)
        {
            auto size = std::max<uint64_t>(info.Buffer.Size, 1);
            if (info.Usage & EBufferUsage::UniformBuffer)
                size = AlignUp(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            return size;
        }

        D3D12_RESOURCE_STATES GetInitialState(const D3D12_HEAP_TYPE heapType)
        {
            return heapType == D3D12_HEAP_TYPE_UPLOAD
                ? D3D12_RESOURCE_STATE_GENERIC_READ
                : D3D12_RESOURCE_STATE_COMMON;
        }

        D3D12_RESOURCE_STATES GetFinalBufferState(const EBufferUsage usage)
        {
            if (usage & EBufferUsage::StorageBuffer)
                return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            if (usage & EBufferUsage::IndexBuffer)
                return D3D12_RESOURCE_STATE_INDEX_BUFFER;
            if (usage & EBufferUsage::VertexBuffer)
                return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            return D3D12_RESOURCE_STATE_COMMON;
        }

        D3D12_RESOURCE_BARRIER GetTransitionBarrier(
            ID3D12Resource* resource,
            const D3D12_RESOURCE_STATES before,
            const D3D12_RESOURCE_STATES after
        )
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = before;
            barrier.Transition.StateAfter = after;
            return barrier;
        }

        D3D12_RESOURCE_FLAGS GetBufferFlags(
            const EBufferUsage usage,
            const D3D12_HEAP_TYPE heapType
        )
        {
            if ((usage & EBufferUsage::StorageBuffer) && heapType == D3D12_HEAP_TYPE_DEFAULT)
                return D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            return D3D12_RESOURCE_FLAG_NONE;
        }
    }

    ID3D12Resource* TryToGetD3D12Buffer(const Buffer* buffer)
    {
        return TryToGetD3D12BufferHandle(buffer);
    }

    template <typename Base>
    template <typename... Args>
    D3D12BaseBuffer<Base>::D3D12BaseBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info,
        Args&&... args
    ) : Base(context, info, std::forward<Args>(args)...)
    {
        m_GraphicsContext = static_cast<const D3D12GraphicsContext*>(context);
    }

    template <typename Base>
    void D3D12BaseBuffer<Base>::Fill(
        const Ref<CommandBuffer>&,
        const uint32_t data,
        const int32_t offset,
        const size_t size
    )
    {
        if constexpr (std::is_base_of_v<DynamicBuffer, Base>)
        {
            if (Base::m_PersistentMapping)
            {
                const auto writeSize = size == 0 ? Base::m_Size - offset : size;
                std::fill_n(
                    reinterpret_cast<uint32_t*>((uint8_t*)Base::m_PersistentMapping + offset),
                    writeSize / sizeof(uint32_t),
                    data
                );
                return;
            }
        }

        EE_CORE_WARN("D3D12 buffer Fill is only implemented for mapped upload buffers.")
    }

    template <typename Base>
    void D3D12BaseBuffer<Base>::Destroy()
    {
        m_Resource.Reset();
    }

    template <typename Base>
    void D3D12BaseBuffer<Base>::Barrier(
        const CommandBuffer* cmd,
        EPipelineStage,
        const EPipelineAccess access
    )
    {
        auto targetState = m_State;

        if (access & (EPipelineAccess::ShaderWrite | EPipelineAccess::ShaderStorageWrite))
            targetState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        else if (access & EPipelineAccess::VertexAttributeRead)
            targetState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        else if (access & EPipelineAccess::IndexRead)
            targetState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
        else if (access & (EPipelineAccess::ShaderRead |
                           EPipelineAccess::ShaderStorageRead |
                           EPipelineAccess::ShaderSampledRead))
            targetState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        const auto d3dCmd = static_cast<const D3D12CommandBuffer*>(cmd);
        auto* commandList = d3dCmd->GetD3D12CommandList();

        if (targetState != m_State)
        {
            const auto barrier = GetTransitionBarrier(m_Resource.Get(), m_State, targetState);
            commandList->ResourceBarrier(1, &barrier);
            m_State = targetState;
            return;
        }

        if (targetState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = m_Resource.Get();
            commandList->ResourceBarrier(1, &barrier);
        }
    }

    template <typename Base>
    void D3D12BaseBuffer<Base>::Copy(
        const CommandBuffer* cmd,
        const Buffer* dst,
        std::span<SBufferCopy> regions
    )
    {
        const auto d3dCmd = static_cast<const D3D12CommandBuffer*>(cmd);
        const auto dstResource = TryToGetD3D12Buffer(dst);

        if (regions.empty())
        {
            const SBufferCopy copy = SBufferCopy::Default(std::min(Base::m_Size, dst->GetSize()));
            regions = std::span<SBufferCopy>((SBufferCopy*)&copy, 1);
            d3dCmd->GetD3D12CommandList()->CopyBufferRegion(
                dstResource,
                copy.DstOffset,
                m_Resource.Get(),
                copy.SrcOffset,
                copy.Size
            );
            return;
        }

        for (const auto& copy : regions)
        {
            d3dCmd->GetD3D12CommandList()->CopyBufferRegion(
                dstResource,
                copy.DstOffset,
                m_Resource.Get(),
                copy.SrcOffset,
                copy.Size
            );
        }
    }

    template <typename Base>
    D3D12_GPU_VIRTUAL_ADDRESS D3D12BaseBuffer<Base>::GetGPUVirtualAddress() const
    {
        return m_Resource ? m_Resource->GetGPUVirtualAddress() : 0;
    }

    template <typename Base>
    D3D12_VERTEX_BUFFER_VIEW D3D12BaseBuffer<Base>::GetVertexBufferView(const uint64_t offset) const
    {
        UINT stride = 0;
        if constexpr (std::is_same_v<Base, VertexBuffer> || std::is_same_v<Base, DynamicVertexBuffer>)
        {
            if (const auto* binding = this->GetLayout().GetBinding(0))
            {
                stride = (UINT)binding->GetStride();
            }
        }

        return {
            .BufferLocation = GetGPUVirtualAddress() + offset,
            .SizeInBytes = (UINT)(Base::m_Size - offset),
            .StrideInBytes = stride,
        };
    }

    template <typename Base>
    D3D12_INDEX_BUFFER_VIEW D3D12BaseBuffer<Base>::GetIndexBufferView(
        const EIndexType type,
        const uint64_t offset
    ) const
    {
        return {
            .BufferLocation = GetGPUVirtualAddress() + offset,
            .SizeInBytes = (UINT)(Base::m_Size - offset),
            .Format = Converters::GetIndexFormat(type),
        };
    }

    template <typename Base>
    void D3D12BaseBuffer<Base>::CreateConstantBufferView(
        const D3D12_CPU_DESCRIPTOR_HANDLE destination
    ) const
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
        desc.BufferLocation = GetGPUVirtualAddress();
        desc.SizeInBytes = (UINT)AlignUp(Base::m_Size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        m_GraphicsContext->GetDevice()->CreateConstantBufferView(&desc, destination);
    }

    template <typename Base>
    void D3D12BaseBuffer<Base>::CreateShaderResourceView(
        const D3D12_CPU_DESCRIPTOR_HANDLE destination,
        const UINT structureStride
    ) const
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = structureStride ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R32_TYPELESS;
        desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.NumElements = structureStride
            ? (UINT)(Base::m_Size / structureStride)
            : (UINT)(AlignUp(Base::m_Size, 4) / 4);
        desc.Buffer.StructureByteStride = structureStride;
        desc.Buffer.Flags = structureStride
            ? D3D12_BUFFER_SRV_FLAG_NONE
            : D3D12_BUFFER_SRV_FLAG_RAW;

        m_GraphicsContext->GetDevice()->CreateShaderResourceView(m_Resource.Get(), &desc, destination);
    }

    template <typename Base>
    void D3D12BaseBuffer<Base>::CreateUnorderedAccessView(
        const D3D12_CPU_DESCRIPTOR_HANDLE destination,
        const UINT structureStride
    ) const
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
        desc.Format = structureStride ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R32_TYPELESS;
        desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.NumElements = structureStride
            ? (UINT)(Base::m_Size / structureStride)
            : (UINT)(AlignUp(Base::m_Size, 4) / 4);
        desc.Buffer.StructureByteStride = structureStride;
        desc.Buffer.CounterOffsetInBytes = 0;
        desc.Buffer.Flags = structureStride
            ? D3D12_BUFFER_UAV_FLAG_NONE
            : D3D12_BUFFER_UAV_FLAG_RAW;

        m_GraphicsContext->GetDevice()->CreateUnorderedAccessView(
            m_Resource.Get(),
            nullptr,
            &desc,
            destination
        );
    }

    template <typename Base>
    void D3D12BaseBuffer<Base>::CreateBuffer(const SBufferCreateInfo& info)
    {
        const auto heapType = GetHeapType(info.AllocationInfo);
        const auto size = GetResourceSize(info);
        m_State = GetInitialState(heapType);

        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = heapType;
        heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc = { 1, 0 };
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = GetBufferFlags(info.Usage, heap.Type);

        DX_CHECK_RESULT(
            m_GraphicsContext->GetDevice()->CreateCommittedResource(
                &heap,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                m_State,
                nullptr,
                IID_PPV_ARGS(&m_Resource)
            )
        );
    }

    template <typename Base>
    void D3D12BaseBuffer<Base>::InitBuffer(const SBuffer& buffer)
    {
        if (buffer.Data)
            UploadInitialData(buffer);
    }

    template <typename Base>
    void D3D12BaseBuffer<Base>::UploadInitialData(const SBuffer& buffer)
    {
        if (!buffer.Data || buffer.Size == 0)
            return;

        if (m_State == D3D12_RESOURCE_STATE_GENERIC_READ)
        {
            void* mapped = nullptr;
            const D3D12_RANGE range = { 0, 0 };
            DX_CHECK_RESULT(m_Resource->Map(0, &range, &mapped));
            Memory::Memcpy(mapped, buffer.Data, buffer.Size);
            m_Resource->Unmap(0, nullptr);
            return;
        }

        D3D12StagingBuffer upload(Base::m_GraphicsContext, buffer.Size, buffer.Data);

        const auto cmd = Base::m_GraphicsContext->GetUploadCommandBuffer();
        cmd->Begin();
        const auto d3dCmd = static_cast<const D3D12CommandBuffer*>(cmd.get());
        const auto commandList = d3dCmd->GetD3D12CommandList();

        if (m_State != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            const auto barrier = GetTransitionBarrier(
                m_Resource.Get(),
                m_State,
                D3D12_RESOURCE_STATE_COPY_DEST
            );
            commandList->ResourceBarrier(1, &barrier);
        }

        commandList->CopyBufferRegion(
            m_Resource.Get(),
            0,
            upload.GetD3D12Resource(),
            0,
            buffer.Size
        );

        const auto finalState = GetFinalBufferState(Base::m_Usage);
        if (finalState != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            const auto barrier = GetTransitionBarrier(
                m_Resource.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                finalState
            );
            commandList->ResourceBarrier(1, &barrier);
        }
        m_State = finalState;
        cmd->Flush();
    }

    template <typename Base>
    template <typename... Args>
    D3D12DynamicBuffer<Base>::D3D12DynamicBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info,
        Args&&... args
    ) : D3D12BaseBuffer<Base>(context, info, std::forward<Args>(args)...) {}

    template <typename Base>
    void* D3D12DynamicBuffer<Base>::Map()
    {
        if (this->m_PersistentMapping)
            return this->m_PersistentMapping;

        const D3D12_RANGE range = { 0, 0 };
        DX_CHECK_RESULT(this->m_Resource->Map(0, &range, &this->m_PersistentMapping));
        return this->m_PersistentMapping;
    }

    template <typename Base>
    void D3D12DynamicBuffer<Base>::Unmap()
    {
        if (!this->m_PersistentMapping)
            return;

        this->m_Resource->Unmap(0, nullptr);
        this->m_PersistentMapping = nullptr;
    }

    template <typename Base>
    void D3D12DynamicBuffer<Base>::InitBuffer(const SBuffer& buffer)
    {
        this->m_PersistentMapping = Map();
        if (buffer.Data)
            Memory::Memcpy(this->m_PersistentMapping, buffer.Data, buffer.Size);
    }

    D3D12Buffer::D3D12Buffer(const GraphicsContext* context, const SBufferCreateInfo& info)
        : D3D12BaseBuffer(context, info)
    {
        CreateBuffer(info);
        InitBuffer(info.Buffer);
    }

    D3D12Buffer::~D3D12Buffer()
    {
        Destroy();
    }

    D3D12StagingBuffer::D3D12StagingBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : D3D12StagingBuffer(context, CreateBufferInfo(size, data)) {}

    D3D12StagingBuffer::D3D12StagingBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : D3D12DynamicBuffer(context, info)
    {
        CreateBuffer(info);
        InitBuffer(info.Buffer);
    }

    D3D12StagingBuffer::~D3D12StagingBuffer()
    {
        if (m_PersistentMapping)
            Unmap();
        Destroy();
    }

    D3D12VertexBuffer::D3D12VertexBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : D3D12VertexBuffer(context, CreateBufferInfo(size, data)) {}

    D3D12VertexBuffer::D3D12VertexBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : D3D12BaseBuffer(context, info)
    {
        CreateBuffer(info);
        InitBuffer(info.Buffer);
        CreateBufferAddress();
    }

    D3D12VertexBuffer::~D3D12VertexBuffer()
    {
        Destroy();
    }

    void D3D12VertexBuffer::CreateBufferAddress()
    {
        m_Address = GetGPUVirtualAddress();
    }

    D3D12DynamicVertexBuffer::D3D12DynamicVertexBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : D3D12DynamicVertexBuffer(context, CreateBufferInfo(size, data)) {}

    D3D12DynamicVertexBuffer::D3D12DynamicVertexBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : D3D12DynamicBuffer(context, info)
    {
        CreateBuffer(info);
        InitBuffer(info.Buffer);
        CreateBufferAddress();
    }

    D3D12DynamicVertexBuffer::~D3D12DynamicVertexBuffer()
    {
        if (m_PersistentMapping)
            Unmap();
        Destroy();
    }

    void D3D12DynamicVertexBuffer::CreateBufferAddress()
    {
        m_Address = GetGPUVirtualAddress();
    }

    D3D12IndexBuffer::D3D12IndexBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data,
        const EIndexType type
    ) : D3D12IndexBuffer(context, CreateBufferInfo(size, data), type) {}

    D3D12IndexBuffer::D3D12IndexBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info,
        const EIndexType type
    ) : D3D12BaseBuffer(context, info, type)
    {
        CreateBuffer(info);
        InitBuffer(info.Buffer);
    }

    D3D12IndexBuffer::~D3D12IndexBuffer()
    {
        Destroy();
    }

    D3D12DynamicIndexBuffer::D3D12DynamicIndexBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data,
        const EIndexType type
    ) : D3D12DynamicIndexBuffer(context, CreateBufferInfo(size, data), type) {}

    D3D12DynamicIndexBuffer::D3D12DynamicIndexBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info,
        const EIndexType type
    ) : D3D12DynamicBuffer(context, info, type)
    {
        CreateBuffer(info);
        InitBuffer(info.Buffer);
    }

    D3D12DynamicIndexBuffer::~D3D12DynamicIndexBuffer()
    {
        if (m_PersistentMapping)
            Unmap();
        Destroy();
    }

    D3D12StorageBuffer::D3D12StorageBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : D3D12StorageBuffer(context, CreateBufferInfo(size, data), data != nullptr) {}

    D3D12StorageBuffer::D3D12StorageBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : D3D12StorageBuffer(context, info, true) {}

    D3D12StorageBuffer::D3D12StorageBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info,
        const bool uploadInitialData
    ) : D3D12BaseBuffer(context, info)
    {
        CreateBuffer(info);
        if (uploadInitialData)
            InitBuffer(info.Buffer);
    }

    D3D12StorageBuffer::~D3D12StorageBuffer()
    {
        Destroy();
    }

    D3D12DynamicStorageBuffer::D3D12DynamicStorageBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : D3D12DynamicStorageBuffer(context, CreateBufferInfo(size, data)) {}

    D3D12DynamicStorageBuffer::D3D12DynamicStorageBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : D3D12DynamicBuffer(context, info)
    {
        CreateBuffer(info);
        InitBuffer(info.Buffer);
    }

    D3D12DynamicStorageBuffer::~D3D12DynamicStorageBuffer()
    {
        if (m_PersistentMapping)
            Unmap();
        Destroy();
    }

    D3D12UniformBuffer::D3D12UniformBuffer(
        const GraphicsContext* context,
        const size_t size,
        const void* data
    ) : D3D12UniformBuffer(context, CreateBufferInfo(size, data)) {}

    D3D12UniformBuffer::D3D12UniformBuffer(
        const GraphicsContext* context,
        const SBufferCreateInfo& info
    ) : D3D12DynamicBuffer(context, info)
    {
        CreateBuffer(info);
        InitBuffer(info.Buffer);
    }

    D3D12UniformBuffer::~D3D12UniformBuffer()
    {
        if (m_PersistentMapping)
            Unmap();
        Destroy();
    }
}

#endif
