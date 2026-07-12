#include "epch.h"
#include "D3D12DescriptorHeap.h"

#ifdef EE_PLATFORM_WINDOWS

namespace Elixir::D3D12
{
    D3D12DescriptorHeap::D3D12DescriptorHeap(
        ID3D12Device* device,
        const D3D12_DESCRIPTOR_HEAP_TYPE type,
        const UINT capacity,
        const bool shaderVisible
    ) : m_Type(type), m_Capacity(capacity), m_ShaderVisible(shaderVisible)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = type;
        desc.NumDescriptors = capacity;
        desc.Flags = shaderVisible
            ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
            : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 0;

        DX_CHECK_RESULT(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap)));
        m_DescriptorSize = device->GetDescriptorHandleIncrementSize(type);
    }

    SD3D12DescriptorAllocation D3D12DescriptorHeap::Allocate(const UINT count)
    {
        std::lock_guard lock(m_Mutex);
        EE_CORE_ASSERT(m_Next + count <= m_Capacity, "D3D12 descriptor heap exhausted!")

        const auto index = m_Next;
        m_Next += count;

        return {
            .CPU = GetCPUHandle(index),
            .GPU = m_ShaderVisible ? GetGPUHandle(index) : D3D12_GPU_DESCRIPTOR_HANDLE{},
            .Index = index,
            .Count = count,
        };
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::GetCPUHandle(const UINT index) const
    {
        auto handle = m_Heap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += (SIZE_T)index * m_DescriptorSize;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::GetGPUHandle(const UINT index) const
    {
        EE_CORE_ASSERT(m_ShaderVisible, "Cannot get GPU handle from CPU-only descriptor heap!")
        auto handle = m_Heap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += (UINT64)index * m_DescriptorSize;
        return handle;
    }
}

#endif
