#pragma once

#include <Engine/Core/Window.h>
#include <Engine/Core/Executor/Executor.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Graphics/Vulkan/VulkanDescriptorAllocator.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Elixir::Vulkan
{
    class VulkanCommandBuffer;
    using Elixir::GraphicsContext;

    class VulkanCommandPoolManager;

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

        void RenderFrame(std::function<void()> callback) override;
        void DrainRenderQueue() override;

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
        VkDescriptorPool GetDescriptorPool(const bool bindless = false) const { return bindless ? m_BindlessDescriptorAllocator->GetPool() : m_DescriptorAllocator->GetPool(); }

        SSwapchainImage& GetCurrentSwapchainImage() { return m_SwapchainImages[m_CurrentSwapchainImageIndex]; }
        VkFormat GetSwapchainImageFormat() const { return m_SwapchainImageFormat; }
        VkExtent3D GetSwapchainVulkanExtent() const { return m_SwapchainExtent; }

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

        void WaitDeviceIdle() const;
        void WaitForAllFrames();

        bool Prepare();
        void Submit();
        void Present();

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
        Ref<VulkanDescriptorAllocator> m_DescriptorAllocator;
        Ref<VulkanDescriptorAllocator> m_BindlessDescriptorAllocator;

        Scope<VulkanCommandPoolManager> m_CommandPoolManager;
        Ref<VulkanCommandBuffer> m_MainCommandBuffer;

        std::vector<SFrameData> m_Frames;
        VkClearColorValue m_ClearColor;

        SDeletionQueue m_DeletionQueue;

        Executor* m_Executor;
        std::atomic<bool> m_AcceptingFrames{true};
        std::counting_semaphore<2> m_FrameSemaphore{2};
    };
}
