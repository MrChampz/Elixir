#pragma once

#include <Engine/Graphics/CommandBuffer.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

namespace Elixir::Vulkan
{
    class VulkanCommandPool
    {
      public:
        explicit VulkanCommandPool(
            const VulkanGraphicsContext* context,
            uint32_t queueFamily,
            bool transient
        );
        ~VulkanCommandPool();

        Ref<CommandBuffer> GetPrimaryCommandBuffer();
        Ref<CommandBuffer> GetSecondaryCommandBuffer();

        void Recycle(const std::vector<Ref<CommandBuffer>>& commands);

        VkCommandPool GetVulkanCommandPool() const { return m_Pool; }

      protected:
        void CreateCommandPool();
        Ref<CommandBuffer> CreateCommandBuffer(ECommandBufferLevel level);

        std::mutex m_Mutex;
        bool m_Transient = false;
        uint32_t m_QueueFamily = 0;
        VkCommandPool m_Pool = VK_NULL_HANDLE;

        std::vector<std::vector<Ref<CommandBuffer>>> m_RecycledCommands;

        const VulkanGraphicsContext* m_GraphicsContext;
    };

    using CommandBufferLookup = std::vector<std::pair<VulkanCommandPool*, std::vector<Ref<CommandBuffer>>>>;

    class VulkanCommandPoolManager
    {
      public:
        explicit VulkanCommandPoolManager(const VulkanGraphicsContext* context);
        ~VulkanCommandPoolManager();

        VulkanCommandPool* GetCommandPool();

        Ref<CommandBuffer> GetPrimaryCommandBuffer();
        Ref<CommandBuffer> GetSecondaryCommandBuffer();

        void EnqueueSecondaryCommandBuffer(const Ref<CommandBuffer>& cmd);

        /**
         * Execute all enqueue secondary CommandBuffers.
         * @param cmd primary CommandBuffer that will be used to execute the secondary ones.
         */
        void FlushSecondaryCommandBuffers(const Ref<CommandBuffer>& cmd);

        /**
         * Enqueue CommandBuffer to be recycled.
         * @param cmd CommandBuffer to be recycled.
         */
        void RecycleCommandBuffer(const Ref<CommandBuffer>& cmd);

        /**
         * Recycles pending command buffers.
         * Should be called after render fence signal.
         */
        void Recycle();

      protected:
        CommandBufferLookup CollectUsedForRecycle(
            const std::vector<Ref<CommandBuffer>>& used
        );

        uint32_t m_GraphicsQueueFamily = 0;
        uint32_t m_TransferQueueFamily = 0;

        std::mutex m_MapMutex;
        std::unordered_map<std::thread::id, Scope<VulkanCommandPool>> m_CommandPools;
        static thread_local VulkanCommandPool* s_ThreadPool;

        std::mutex m_UploadMutex;
        Scope<VulkanCommandPool> m_UploadPool;
        std::queue<std::pair<VkFence, Ref<CommandBuffer>>> m_UploadQueue;

        std::mutex m_CommandsMutex;
        std::queue<Ref<CommandBuffer>> m_CommandsQueue;

        /**
         * Collection of CommandBuffers queued for recycling.
         * The vector is indexed by frame index.
         */
        std::vector<std::vector<Ref<CommandBuffer>>> m_PendingRecycles;

        const VulkanGraphicsContext* m_GraphicsContext;
    };
}