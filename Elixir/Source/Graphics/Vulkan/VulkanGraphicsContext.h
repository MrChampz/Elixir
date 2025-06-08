#pragma once

#include <Engine/Core/Window.h>
#include <Engine/Graphics/GraphicsContext.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Elixir::Vulkan
{
    using Elixir::GraphicsContext;

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
            for (auto it = Deletors.rbegin(); it != Deletors.rend(); it++)
            {
                (*it)();
            }

            Deletors.clear();
        }
    };

    struct SFrameData
    {
        VkSemaphore SwapchainSemaphore, RenderSemaphore;
        VkFence RenderFence;

        SDeletionQueue DeletionQueue;
    };

    class ELIXIR_API VulkanGraphicsContext final : public GraphicsContext
    {
      public:
        explicit VulkanGraphicsContext(const EGraphicsAPI api, const Window* window);
        ~VulkanGraphicsContext() override;

        void Init() override;
        void Shutdown() override;

        void Prepare() override;
        void Submit() override;
        void Present() override;

        void WaitDeviceIdle() override;

        void SetClearColor(const glm::vec4& color) override;
        void Clear() override;

        void Resize(Extent2D extent) override;

        void FlushCommandBuffer(CommandBuffer& cmd) const override;

        Extent2D GetSwapchainExtent() const override;

        SFrameData& GetCurrentFrame() { return m_Frames[m_FrameNumber % FRAMES]; }

        VkInstance GetInstance() const { return m_Instance; }
        VkPhysicalDevice GetGPU() const { return m_GPU; }
        VkDevice GetDevice() const { return m_Device; }
        VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
        uint32_t GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
        VmaAllocator GetAllocator() const { return m_Allocator; }
        const VkDescriptorPool& GetDescriptorPool() const { return m_GlobalDescriptorAllocator.Pool; }

        VkFormat GetSwapchainImageFormat() const { return m_SwapchainImageFormat; }
        VkExtent2D GetSwapchainVulkanExtent() const { return m_SwapchainExtent; }

    private:
        void InitVulkan();
        void InitAllocator();
        void InitSwapchain();
        void InitCommands();
        void InitSyncStructures();
        void InitDescriptors();

        void CreateSwapchain(Extent2D extent);
        void DestroySwapchain();
        void RecreateSwapchain();

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
        VkExtent2D m_SwapchainExtent;
        //std::vector<Ref<VulkanTexture2D>> m_SwapchainImages;
        uint32_t m_CurrentSwapchainImageIndex = 0;
        bool m_SwapchainRecreateRequested = false;

        VkQueue m_GraphicsQueue;
        uint32_t m_GraphicsQueueFamily;

        VkQueue m_TransferQueue;
        uint32_t m_TransferQueueFamily;

        VmaAllocator m_Allocator;
        SDescriptorAllocator m_GlobalDescriptorAllocator;

        SFrameData m_Frames[FRAMES];
        VkClearColorValue m_ClearColor;

        SDeletionQueue m_DeletionQueue;
    };
}
