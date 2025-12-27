#pragma once

#include <Engine/Core/Window.h>
#include <Engine/Core/Executor/Executor.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Graphics/Vulkan/VulkanTexture.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Elixir::Vulkan
{
    class VulkanCommandBuffer;
    using Elixir::GraphicsContext;

    class VulkanCommandPoolManager;

    struct SDescriptorAllocator
    {
        struct SPoolSizeRatio
        {
            VkDescriptorType Type;
            float Ratio;
        };

        VkDescriptorPool Pool;

        void InitPool(VkDevice device, uint32_t maxSets, std::span<SPoolSizeRatio> ratios);
        void Reset(VkDevice device) const;
        void DestroyPool(VkDevice device) const;

        VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout) const;
    };

    // TODO: Move to Core?
    struct SDeletionQueue
    {
        std::deque<std::function<void()>> Deletors;

        void Push(std::function<void()>&& fn)
        {
            Deletors.push_back(fn);
        }

        void Flush()
        {
            // Reverse iterate the deletion queue to execute in the correct order.
            for (auto it = Deletors.rbegin(); it != Deletors.rend(); ++it)
            {
                (*it)();
            }

            Deletors.clear();
        }
    };

    struct SFrameData
    {
        VkSemaphore SwapchainSemaphore;
        VkFence RenderFence;

        SDeletionQueue DeletionQueue;

        std::atomic<bool> InUseByRenderThread{false};

        SFrameData() = default;
        SFrameData(const SFrameData&) = delete;
        SFrameData(SFrameData&& other) noexcept
            : SwapchainSemaphore(other.SwapchainSemaphore), RenderFence(other.RenderFence),
              DeletionQueue(std::move(other.DeletionQueue)),
              InUseByRenderThread(other.InUseByRenderThread.load())
        {
            other.SwapchainSemaphore = VK_NULL_HANDLE;
            other.RenderFence = VK_NULL_HANDLE;
        }

        SFrameData& operator=(const SFrameData&) = delete;
    };

    struct SSwapchainImage
    {
        VkImage Image = VK_NULL_HANDLE;
        VkImageView View = VK_NULL_HANDLE;
        VkSemaphore RenderSemaphore = VK_NULL_HANDLE;
    };

    class ELIXIR_API VulkanGraphicsContext final : public GraphicsContext
    {
      public:
        explicit VulkanGraphicsContext(EGraphicsAPI api, Executor* executor, const Window* window);
        ~VulkanGraphicsContext() override;

        void Init() override;
        void Shutdown() override;

        bool Prepare() override;
        void Submit() override;
        void Present() override;

        void WaitDeviceIdle() override;

        void SetClearColor(const glm::vec4& color) override;
        void Clear() override;

        void Resize(Extent2D extent) override;

        Ref<CommandBuffer> GetSecondaryCommandBuffer() const override;
        Ref<CommandBuffer> GetUploadCommandBuffer() const override;
        void EnqueueSecondaryCommandBuffer(const Ref<CommandBuffer>& cmd) const override;

        Extent2D GetSwapchainExtent() const override;

        SFrameData& GetCurrentFrame() { return m_Frames[GetFrameIndex()]; }

        VkInstance GetInstance() const { return m_Instance; }
        VkPhysicalDevice GetGPU() const { return m_GPU; }
        VkDevice GetDevice() const { return m_Device; }
        VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
        uint32_t GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
        VkQueue GetTransferQueue() const { return m_TransferQueue; }
        uint32_t GetTransferQueueFamily() const { return m_TransferQueueFamily; }
        VmaAllocator GetAllocator() const { return m_Allocator; }
        const VkDescriptorPool& GetDescriptorPool() const { return m_GlobalDescriptorAllocator.Pool; }

        SSwapchainImage& GetCurrentSwapchainImage() { return m_SwapchainImages[m_CurrentSwapchainImageIndex]; }
        VkFormat GetSwapchainImageFormat() const { return m_SwapchainImageFormat; }
        VkExtent3D GetSwapchainVulkanExtent() const { return m_SwapchainExtent; }

        void BeginFrame() override;
        void RenderFrame(std::function<void()> callback) override;
        void WaitForFrame() override;
        void WaitForAllFrames() override;
        void FlushAndWait() override;

      private:
        void InitVulkan();
        void InitAllocator();
        void InitSwapchain();
        void InitCommandPoolManager();
        void InitSyncStructures();
        void InitDescriptors();

        void CreateSwapchain(const Extent2D& extent);
        void DestroySwapchain();
        void RecreateSwapchain();

        void CreateRenderTarget() override;

        bool m_IsInitialized = false;

#ifdef EE_DEBUG
        bool m_UseValidationLayers = true;
#else
        bool m_UseValidationLayers = false;
#endif

        Extent2D m_WindowExtent;

        VkInstance m_Instance;
        VkDebugUtilsMessengerEXT m_DebugMessenger;
        VkPhysicalDevice m_GPU;
        VkPhysicalDeviceProperties m_GPUProperties;
        VkDevice m_Device;
        VkSurfaceKHR m_Surface;

        VkSwapchainKHR m_Swapchain;
        VkFormat m_SwapchainImageFormat;
        VkExtent3D m_SwapchainExtent;
        std::vector<SSwapchainImage> m_SwapchainImages;
        uint32_t m_CurrentSwapchainImageIndex = 0;
        bool m_SwapchainRecreateRequested = false;

        VkQueue m_GraphicsQueue;
        uint32_t m_GraphicsQueueFamily;

        VkQueue m_TransferQueue;
        uint32_t m_TransferQueueFamily;

        VmaAllocator m_Allocator;
        SDescriptorAllocator m_GlobalDescriptorAllocator;

        Scope<VulkanCommandPoolManager> m_CommandPoolManager;
        Ref<VulkanCommandBuffer> m_MainCommandBuffer;

        std::vector<SFrameData> m_Frames;
        VkClearColorValue m_ClearColor;

        SDeletionQueue m_DeletionQueue;

        Executor* m_Executor;

        // Frame completion tracking
        //std::atomic<bool> m_FrameInFlight{false};
        WaitGroup m_RenderWaitGroup;
        std::atomic<bool> m_AcceptingFrames{true};
    };
}
