#pragma once

#ifdef EE_PLATFORM_WINDOWS

#include <Graphics/D3D12/D3D12Utils.h>

namespace Elixir::D3D12
{
    struct SD3D12DescriptorAllocation
    {
        D3D12_CPU_DESCRIPTOR_HANDLE CPU = {};
        D3D12_GPU_DESCRIPTOR_HANDLE GPU = {};
        UINT Index = UINT_MAX;
        UINT Count = 0;

        bool IsValid() const { return Index != UINT_MAX; }
    };

    class D3D12DescriptorHeap final
    {
      public:
        D3D12DescriptorHeap(
            ID3D12Device* device,
            D3D12_DESCRIPTOR_HEAP_TYPE type,
            UINT capacity,
            bool shaderVisible
        );

        SD3D12DescriptorAllocation Allocate(UINT count = 1);

        D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT index) const;

        ID3D12DescriptorHeap* GetHeap() const { return m_Heap.Get(); }
        UINT GetDescriptorSize() const { return m_DescriptorSize; }
        UINT GetCapacity() const { return m_Capacity; }

      private:
        ComPtr<ID3D12DescriptorHeap> m_Heap;
        D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
        UINT m_DescriptorSize = 0;
        UINT m_Capacity = 0;
        UINT m_Next = 0;
        bool m_ShaderVisible = false;
        mutable std::mutex m_Mutex;
    };
}

#endif
