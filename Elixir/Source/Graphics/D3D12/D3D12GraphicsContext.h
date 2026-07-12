#pragma once

#ifdef EE_PLATFORM_WINDOWS

#include <Engine/Core/DeletionQueue.h>
#include <Engine/Core/Executor/Executor.h>
#include <Engine/Core/Window.h>
#include <Engine/Event/WindowEvent.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Graphics/D3D12/D3D12DescriptorHeap.h>

namespace Elixir::D3D12
{
    class D3D12CommandBuffer;

    struct SD3D12SwapchainImage
    {
        ComPtr<ID3D12Resource> Resource;
        D3D12_CPU_DESCRIPTOR_HANDLE RTV = {};
    };

    class ELIXIR_API D3D12GraphicsContext final : public GraphicsContext
    {
      public:
        static constexpr UINT RESOURCE_SRV_BASE = 0;
        static constexpr UINT RESOURCE_SRV_COUNT = 2048;
        static constexpr UINT RESOURCE_CBV_BASE = RESOURCE_SRV_BASE + RESOURCE_SRV_COUNT;
        static constexpr UINT RESOURCE_CBV_COUNT = 512;
        static constexpr UINT RESOURCE_UAV_BASE = RESOURCE_CBV_BASE + RESOURCE_CBV_COUNT;
        static constexpr UINT RESOURCE_UAV_COUNT = 512;
        static constexpr UINT RESOURCE_HEAP_SIZE = RESOURCE_UAV_BASE + RESOURCE_UAV_COUNT;
        static constexpr UINT SAMPLER_HEAP_SIZE = 256;

        explicit D3D12GraphicsContext(EGraphicsAPI api, Executor* executor, const Window* window);
        ~D3D12GraphicsContext() override;

        void Init() override;
        void Shutdown() override;

        void ProcessEvent(Event& event) override;

        void RenderFrame(std::function<void()> callback) override;
        void DrainRenderQueue() override;

        void SetClearColor(const glm::vec4& color) override;
        void Clear() override;

        void Resize(Extent2D extent) override;

        Ref<CommandBuffer> GetSecondaryCommandBuffer() const override;
        Ref<CommandBuffer> GetUploadCommandBuffer() const override;
        void EnqueueSecondaryCommandBuffer(const Ref<CommandBuffer>& cmd) const override;

        Extent3D GetSwapchainExtent() const override { return m_SwapchainExtent; }

        ID3D12Device* GetDevice() const { return m_Device.Get(); }
        ID3D12CommandQueue* GetGraphicsQueue() const { return m_GraphicsQueue.Get(); }

        D3D12DescriptorHeap& GetResourceDescriptorHeap() const { return *m_ResourceDescriptorHeap; }
        D3D12DescriptorHeap& GetSamplerDescriptorHeap() const { return *m_SamplerDescriptorHeap; }

        D3D12_CPU_DESCRIPTOR_HANDLE GetResourceCPUHandle(UINT index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetResourceGPUHandle(UINT index) const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetSamplerCPUHandle(UINT index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerGPUHandle(UINT index) const;

        SD3D12DescriptorAllocation AllocateRTVDescriptor(UINT count = 1) const;
        SD3D12DescriptorAllocation AllocateDSVDescriptor(UINT count = 1) const;
        SD3D12DescriptorAllocation AllocateShaderResourceDescriptor(UINT count = 1) const;
        SD3D12DescriptorAllocation AllocateSamplerDescriptor(UINT count = 1) const;

        void ExecuteCommandListsAndWait(std::span<ID3D12CommandList*> commandLists) const;

      private:
        void InitD3D12();
        void InitDescriptorHeaps();
        void InitSwapchain();
        void InitSyncStructures();

        void PickAdapter();
        void CreateDevice();
        void CreateCommandQueue();
        void CreateSwapchain(const Extent3D& extent);
        void CreateSwapchainRenderTargets();
        void DestroySwapchain();
        void RecreateSwapchain();
        void CreateRenderTargets() override;

        bool Prepare();
        void Submit();
        void Present();

        void WaitForGPU() const;

        bool HandleFramebufferResize(const FramebufferResizeEvent& event);

        bool m_IsInitialized = false;
        bool m_SwapchainRecreateRequested = false;

        ComPtr<IDXGIFactory6> m_Factory;
        ComPtr<IDXGIAdapter1> m_Adapter;
        ComPtr<ID3D12Device> m_Device;
#ifdef EE_DEBUG
        ComPtr<ID3D12InfoQueue1> m_InfoQueue;
        DWORD m_InfoQueueCallbackCookie = 0;
        bool m_InfoQueueCallbackRegistered = false;
#endif
        ComPtr<ID3D12CommandQueue> m_GraphicsQueue;
        ComPtr<IDXGISwapChain3> m_Swapchain;

        DXGI_FORMAT m_SwapchainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        Extent3D m_SwapchainExtent = {};
        std::array<SD3D12SwapchainImage, FRAMES> m_SwapchainImages;
        UINT m_CurrentBackBufferIndex = 0;

        Scope<D3D12DescriptorHeap> m_RTVHeap;
        Scope<D3D12DescriptorHeap> m_DSVHeap;
        Scope<D3D12DescriptorHeap> m_ResourceDescriptorHeap;
        Scope<D3D12DescriptorHeap> m_SamplerDescriptorHeap;

        Ref<D3D12CommandBuffer> m_PreFrameCommandBuffer;
        Ref<D3D12CommandBuffer> m_PostFrameCommandBuffer;
        mutable std::mutex m_QueuedCommandBuffersMutex;
        mutable std::vector<Ref<CommandBuffer>> m_QueuedCommandBuffers;

        ComPtr<ID3D12Fence> m_Fence;
        mutable uint64_t m_FenceValue = 0;
        HANDLE m_FenceEvent = nullptr;
        mutable std::mutex m_FenceMutex;

        float m_ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

        Executor* m_Executor = nullptr;
        std::atomic<bool> m_AcceptingFrames{true};
        std::counting_semaphore<2> m_FrameSemaphore{2};
    };
}

#endif
