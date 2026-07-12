#include "epch.h"
#include "D3D12GraphicsContext.h"

#ifdef EE_PLATFORM_WINDOWS

#include <Graphics/D3D12/D3D12CommandBuffer.h>
#include <Graphics/D3D12/D3D12Image.h>
#include <Graphics/D3D12/D3D12ShaderBackend.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace Elixir::D3D12
{
    namespace
    {
#ifdef EE_DEBUG
        void CALLBACK LogD3D12Message(
            D3D12_MESSAGE_CATEGORY,
            const D3D12_MESSAGE_SEVERITY severity,
            const D3D12_MESSAGE_ID id,
            const char* description,
            void*
        )
        {
            if (severity == D3D12_MESSAGE_SEVERITY_CORRUPTION ||
                severity == D3D12_MESSAGE_SEVERITY_ERROR)
            {
                EE_CORE_ERROR("D3D12 validation [{0}]: {1}", (uint32_t)id, description)
            }
            else if (severity == D3D12_MESSAGE_SEVERITY_WARNING)
            {
                EE_CORE_WARN("D3D12 validation [{0}]: {1}", (uint32_t)id, description)
            }
        }
#endif
    }

    D3D12GraphicsContext::D3D12GraphicsContext(
        const EGraphicsAPI api,
        Executor* executor,
        const Window* window
    ) : GraphicsContext(api, window), m_Executor(executor)
    {
        EE_CORE_ASSERT(executor, "Invalid executor!")
        EE_CORE_ASSERT(window, "Invalid window!")

        m_ShaderBackend = CreateScope<D3D12ShaderBackend>();
    }

    D3D12GraphicsContext::~D3D12GraphicsContext()
    {
        Shutdown();
    }

    void D3D12GraphicsContext::Init()
    {
        InitD3D12();
        InitDescriptorHeaps();
        InitSwapchain();
        InitSyncStructures();
        CreateRenderTargets();

        SetClearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
        m_IsInitialized = true;
    }

    void D3D12GraphicsContext::Shutdown()
    {
        if (!m_IsInitialized)
            return;

        DrainRenderQueue();
        WaitForGPU();

        m_PreFrameCommandBuffer.reset();
        m_PostFrameCommandBuffer.reset();

        m_RenderTarget.reset();
        m_DepthStencilRenderTarget.reset();
        DestroySwapchain();

        if (m_FenceEvent)
        {
            CloseHandle(m_FenceEvent);
            m_FenceEvent = nullptr;
        }

        m_Fence.Reset();
        m_SamplerDescriptorHeap.reset();
        m_ResourceDescriptorHeap.reset();
        m_DSVHeap.reset();
        m_RTVHeap.reset();
        m_Swapchain.Reset();
        m_GraphicsQueue.Reset();
#ifdef EE_DEBUG
        if (m_InfoQueueCallbackRegistered)
            m_InfoQueue->UnregisterMessageCallback(m_InfoQueueCallbackCookie);
        m_InfoQueueCallbackRegistered = false;
        m_InfoQueue.Reset();
#endif
        m_Device.Reset();
        m_Adapter.Reset();
        m_Factory.Reset();

        m_IsInitialized = false;
    }

    void D3D12GraphicsContext::ProcessEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<FramebufferResizeEvent>(
            EE_BIND_EVENT_FN(D3D12GraphicsContext::HandleFramebufferResize)
        );
    }

    void D3D12GraphicsContext::RenderFrame(std::function<void()> callback)
    {
        if (!m_AcceptingFrames.load())
            return;

        m_FrameSemaphore.acquire();

        m_Executor->Enqueue(EThreadName::Rendering, [this, callback]()
        {
            if (!Prepare())
            {
                m_FrameSemaphore.release();
                return;
            }

            if (callback)
                callback();

            Submit();
            Present();

            m_FrameSemaphore.release();
        });
    }

    void D3D12GraphicsContext::DrainRenderQueue()
    {
        if (!m_IsInitialized)
            return;

        m_AcceptingFrames = false;
        m_Executor->ShutdownRenderPool();
        WaitForGPU();
    }

    void D3D12GraphicsContext::SetClearColor(const glm::vec4& color)
    {
        m_ClearColor[0] = color.r;
        m_ClearColor[1] = color.g;
        m_ClearColor[2] = color.b;
        m_ClearColor[3] = color.a;
    }

    void D3D12GraphicsContext::Clear()
    {
        if (!m_PreFrameCommandBuffer)
            return;

        m_RenderTarget->Transition(m_PreFrameCommandBuffer.get(), EImageLayout::ColorAttachment);

        const auto target = dynamic_cast<D3D12BaseImage<Texture2D>*>(m_RenderTarget.get());
        EE_CORE_ASSERT(target, "D3D12 render target is invalid!")

        m_PreFrameCommandBuffer->GetD3D12CommandList()->ClearRenderTargetView(
            target->GetRenderTargetView(),
            m_ClearColor,
            0,
            nullptr
        );
    }

    void D3D12GraphicsContext::Resize(const Extent2D extent)
    {
        if (extent.Width == 0 || extent.Height == 0)
            return;

        m_SwapchainExtent = extent;
        m_SwapchainRecreateRequested = true;

        const auto cmd = GetUploadCommandBuffer();
        m_RenderTarget->Resize(cmd, extent);
        m_DepthStencilRenderTarget->Resize(cmd, extent);
    }

    Ref<CommandBuffer> D3D12GraphicsContext::GetSecondaryCommandBuffer() const
    {
        return CreateRef<D3D12CommandBuffer>(this, ECommandBufferLevel::Secondary);
    }

    Ref<CommandBuffer> D3D12GraphicsContext::GetUploadCommandBuffer() const
    {
        return CreateRef<D3D12CommandBuffer>(this, ECommandBufferLevel::Primary);
    }

    void D3D12GraphicsContext::EnqueueSecondaryCommandBuffer(const Ref<CommandBuffer>& cmd) const
    {
        std::lock_guard lock(m_QueuedCommandBuffersMutex);
        m_QueuedCommandBuffers.push_back(cmd);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsContext::GetResourceCPUHandle(const UINT index) const
    {
        return m_ResourceDescriptorHeap->GetCPUHandle(index);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12GraphicsContext::GetResourceGPUHandle(const UINT index) const
    {
        return m_ResourceDescriptorHeap->GetGPUHandle(index);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsContext::GetSamplerCPUHandle(const UINT index) const
    {
        return m_SamplerDescriptorHeap->GetCPUHandle(index);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12GraphicsContext::GetSamplerGPUHandle(const UINT index) const
    {
        return m_SamplerDescriptorHeap->GetGPUHandle(index);
    }

    SD3D12DescriptorAllocation D3D12GraphicsContext::AllocateRTVDescriptor(const UINT count) const
    {
        return m_RTVHeap->Allocate(count);
    }

    SD3D12DescriptorAllocation D3D12GraphicsContext::AllocateDSVDescriptor(const UINT count) const
    {
        return m_DSVHeap->Allocate(count);
    }

    SD3D12DescriptorAllocation D3D12GraphicsContext::AllocateShaderResourceDescriptor(
        const UINT count
    ) const
    {
        return m_ResourceDescriptorHeap->Allocate(count);
    }

    SD3D12DescriptorAllocation D3D12GraphicsContext::AllocateSamplerDescriptor(
        const UINT count
    ) const
    {
        return m_SamplerDescriptorHeap->Allocate(count);
    }

    void D3D12GraphicsContext::ExecuteCommandListsAndWait(
        const std::span<ID3D12CommandList*> commandLists
    ) const
    {
        if (commandLists.empty())
            return;

        m_GraphicsQueue->ExecuteCommandLists((UINT)commandLists.size(), commandLists.data());
        WaitForGPU();
    }

    void D3D12GraphicsContext::InitD3D12()
    {
#ifdef EE_DEBUG
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            debug->EnableDebugLayer();
#endif

        UINT factoryFlags = 0;
#ifdef EE_DEBUG
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

        DX_CHECK_RESULT(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_Factory)));

        PickAdapter();
        CreateDevice();
        CreateCommandQueue();

        DXGI_ADAPTER_DESC1 desc = {};
        m_Adapter->GetDesc1(&desc);
        const std::wstring wideName(desc.Description);
        const std::string adapterName(wideName.begin(), wideName.end());
        EE_CORE_INFO("D3D12 Renderer:")
        EE_CORE_INFO("  Adapter: {0}", adapterName)
        EE_CORE_INFO("  Dedicated VRAM: {0} MB", desc.DedicatedVideoMemory / (1024 * 1024))
    }

    void D3D12GraphicsContext::InitDescriptorHeaps()
    {
        m_RTVHeap = CreateScope<D3D12DescriptorHeap>(
            m_Device.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            64,
            false
        );
        m_DSVHeap = CreateScope<D3D12DescriptorHeap>(
            m_Device.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            64,
            false
        );
        m_ResourceDescriptorHeap = CreateScope<D3D12DescriptorHeap>(
            m_Device.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            RESOURCE_HEAP_SIZE,
            true
        );
        m_SamplerDescriptorHeap = CreateScope<D3D12DescriptorHeap>(
            m_Device.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
            SAMPLER_HEAP_SIZE,
            true
        );
    }

    void D3D12GraphicsContext::InitSwapchain()
    {
        m_SwapchainExtent = m_Window->GetFramebufferExtent();
        CreateSwapchain(m_SwapchainExtent);
    }

    void D3D12GraphicsContext::InitSyncStructures()
    {
        DX_CHECK_RESULT(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
        m_FenceValue = 0;
        m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        EE_CORE_ASSERT(m_FenceEvent, "Failed to create D3D12 fence event!")
    }

    void D3D12GraphicsContext::PickAdapter()
    {
        for (UINT index = 0;; ++index)
        {
            ComPtr<IDXGIAdapter1> adapter;
            const auto result = m_Factory->EnumAdapterByGpuPreference(
                index,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&adapter)
            );

            if (result == DXGI_ERROR_NOT_FOUND)
                break;

            DXGI_ADAPTER_DESC1 desc = {};
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
            {
                m_Adapter = adapter;
                return;
            }
        }

        EE_CORE_ASSERT(false, "No suitable D3D12 adapter found!")
    }

    void D3D12GraphicsContext::CreateDevice()
    {
        DX_CHECK_RESULT(
            D3D12CreateDevice(
                m_Adapter.Get(),
                D3D_FEATURE_LEVEL_12_0,
                IID_PPV_ARGS(&m_Device)
            )
        );

#ifdef EE_DEBUG
        if (SUCCEEDED(m_Device.As(&m_InfoQueue)))
        {
            m_InfoQueueCallbackRegistered = SUCCEEDED(
                m_InfoQueue->RegisterMessageCallback(
                    LogD3D12Message,
                    D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                    nullptr,
                    &m_InfoQueueCallbackCookie
                )
            );
        }
#endif
    }

    void D3D12GraphicsContext::CreateCommandQueue()
    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 0;
        DX_CHECK_RESULT(m_Device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_GraphicsQueue)));
    }

    void D3D12GraphicsContext::CreateSwapchain(const Extent3D& extent)
    {
        if (extent.Width == 0 || extent.Height == 0)
            return;

        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = extent.Width;
        desc.Height = extent.Height;
        desc.Format = m_SwapchainFormat;
        desc.Stereo = FALSE;
        desc.SampleDesc = { 1, 0 };
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = FRAMES;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        desc.Flags = 0;

        ComPtr<IDXGISwapChain1> swapchain;
        DX_CHECK_RESULT(
            m_Factory->CreateSwapChainForHwnd(
                m_GraphicsQueue.Get(),
                glfwGetWin32Window(static_cast<GLFWwindow*>(m_Window->GetHandle())),
                &desc,
                nullptr,
                nullptr,
                &swapchain
            )
        );

        DX_CHECK_RESULT(m_Factory->MakeWindowAssociation(
            glfwGetWin32Window(static_cast<GLFWwindow*>(m_Window->GetHandle())),
            DXGI_MWA_NO_ALT_ENTER
        ));

        DX_CHECK_RESULT(swapchain.As(&m_Swapchain));
        m_CurrentBackBufferIndex = m_Swapchain->GetCurrentBackBufferIndex();
        m_SwapchainExtent = extent;

        CreateSwapchainRenderTargets();
    }

    void D3D12GraphicsContext::CreateSwapchainRenderTargets()
    {
        for (UINT i = 0; i < FRAMES; ++i)
        {
            DX_CHECK_RESULT(m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&m_SwapchainImages[i].Resource)));
            const auto allocation = AllocateRTVDescriptor();
            m_SwapchainImages[i].RTV = allocation.CPU;
            m_Device->CreateRenderTargetView(
                m_SwapchainImages[i].Resource.Get(),
                nullptr,
                m_SwapchainImages[i].RTV
            );
        }
    }

    void D3D12GraphicsContext::DestroySwapchain()
    {
        for (auto& image : m_SwapchainImages)
        {
            image.Resource.Reset();
            image.RTV = {};
        }

        m_Swapchain.Reset();
    }

    void D3D12GraphicsContext::RecreateSwapchain()
    {
        WaitForGPU();

        for (auto& image : m_SwapchainImages)
            image.Resource.Reset();

        DX_CHECK_RESULT(
            m_Swapchain->ResizeBuffers(
                FRAMES,
                m_SwapchainExtent.Width,
                m_SwapchainExtent.Height,
                m_SwapchainFormat,
                0
            )
        );

        m_CurrentBackBufferIndex = m_Swapchain->GetCurrentBackBufferIndex();
        CreateSwapchainRenderTargets();
        m_SwapchainRecreateRequested = false;
    }

    void D3D12GraphicsContext::CreateRenderTargets()
    {
        auto colorInfo = Texture2D::CreateImageInfo(
            EImageFormat::R8G8B8A8_SRGB,
            m_SwapchainExtent.Width,
            m_SwapchainExtent.Height
        );
        colorInfo.Usage = EImageUsage::ColorAttachment | EImageUsage::TransferSrc | EImageUsage::TransferDst | EImageUsage::Sampled;
        colorInfo.InitialLayout = EImageLayout::ColorAttachment;
        m_RenderTarget = CreateRef<D3D12Texture2D>(this, colorInfo);

        auto depthInfo = DepthStencilImage::CreateImageInfo(
            EDepthStencilImageFormat::D32_SFLOAT,
            m_SwapchainExtent.Width,
            m_SwapchainExtent.Height
        );
        depthInfo.InitialLayout = EImageLayout::DepthStencilAttachment;
        m_DepthStencilRenderTarget = CreateRef<D3D12DepthStencilImage>(this, depthInfo);
    }

    bool D3D12GraphicsContext::Prepare()
    {
        if (m_SwapchainRecreateRequested)
            RecreateSwapchain();

        {
            std::lock_guard lock(m_QueuedCommandBuffersMutex);
            m_QueuedCommandBuffers.clear();
        }

        m_CurrentBackBufferIndex = m_Swapchain->GetCurrentBackBufferIndex();
        m_PreFrameCommandBuffer = CreateRef<D3D12CommandBuffer>(this, ECommandBufferLevel::Primary);
        m_PreFrameCommandBuffer->Begin();
        return true;
    }

    void D3D12GraphicsContext::Submit()
    {
        std::vector<Ref<CommandBuffer>> queued;
        {
            std::lock_guard lock(m_QueuedCommandBuffersMutex);
            queued.swap(m_QueuedCommandBuffers);
        }

        std::vector<ID3D12CommandList*> lists;
        lists.reserve(queued.size() + 2);

        m_PreFrameCommandBuffer->End();
        lists.push_back(m_PreFrameCommandBuffer->GetD3D12CommandList());

        for (const auto& cmd : queued)
        {
            cmd->End();
            const auto d3dCmd = std::static_pointer_cast<D3D12CommandBuffer>(cmd);
            lists.push_back(d3dCmd->GetD3D12CommandList());
        }

        m_PostFrameCommandBuffer = CreateRef<D3D12CommandBuffer>(this, ECommandBufferLevel::Primary);
        m_PostFrameCommandBuffer->Begin();

        m_RenderTarget->Transition(m_PostFrameCommandBuffer.get(), EImageLayout::TransferSrc);

        auto& swapchainImage = m_SwapchainImages[m_CurrentBackBufferIndex];
        D3D12_RESOURCE_BARRIER toCopy = {};
        toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopy.Transition.pResource = swapchainImage.Resource.Get();
        toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

        m_PostFrameCommandBuffer->GetD3D12CommandList()->ResourceBarrier(1, &toCopy);
        m_PostFrameCommandBuffer->GetD3D12CommandList()->CopyResource(
            swapchainImage.Resource.Get(),
            TryToGetD3D12ImageResource(m_RenderTarget.get())
        );

        std::swap(toCopy.Transition.StateBefore, toCopy.Transition.StateAfter);
        m_PostFrameCommandBuffer->GetD3D12CommandList()->ResourceBarrier(1, &toCopy);

        m_PostFrameCommandBuffer->End();
        lists.push_back(m_PostFrameCommandBuffer->GetD3D12CommandList());

        m_GraphicsQueue->ExecuteCommandLists((UINT)lists.size(), lists.data());
        WaitForGPU();
    }

    void D3D12GraphicsContext::Present()
    {
        const auto syncInterval = m_VSyncEnabled ? 1u : 0u;
        const auto result = m_Swapchain->Present(syncInterval, 0);

        if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
        {
            EE_CORE_ASSERT(false, "D3D12 device was removed or reset!")
            return;
        }

        DX_CHECK_RESULT(result);
        m_CurrentBackBufferIndex = m_Swapchain->GetCurrentBackBufferIndex();
        m_FrameNumber++;
    }

    void D3D12GraphicsContext::WaitForGPU() const
    {
        std::lock_guard lock(m_FenceMutex);
        const auto fenceValue = ++m_FenceValue;
        DX_CHECK_RESULT(m_GraphicsQueue->Signal(m_Fence.Get(), fenceValue));

        if (m_Fence->GetCompletedValue() < fenceValue)
        {
            DX_CHECK_RESULT(m_Fence->SetEventOnCompletion(fenceValue, m_FenceEvent));
            constexpr DWORD timeoutMs = 10000;
            const auto waitResult = WaitForSingleObject(m_FenceEvent, timeoutMs);
            if (waitResult == WAIT_TIMEOUT)
            {
                const auto removedReason = m_Device->GetDeviceRemovedReason();
                EE_CORE_ERROR(
                    "D3D12 GPU wait timed out after {0} ms (device reason=0x{1:08X})",
                    timeoutMs,
                    (uint32_t)removedReason
                )
                DEBUG_BREAK()
            }
            else if (waitResult != WAIT_OBJECT_0)
            {
                EE_CORE_ERROR("D3D12 GPU wait failed (Win32 error={0})", GetLastError())
                DEBUG_BREAK()
            }
        }
    }

    bool D3D12GraphicsContext::HandleFramebufferResize(const FramebufferResizeEvent& event)
    {
        Resize(event.GetExtent());
        return true;
    }
}

#endif
