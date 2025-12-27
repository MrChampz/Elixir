#include "epch.h"
#include "VulkanCommandPool.h"

#include "VulkanCommandBuffer.h"
#include "Utils.h"

namespace Elixir::Vulkan
{
    /* VulkanCommandPool */
    VulkanCommandPool::VulkanCommandPool(
        const VulkanGraphicsContext* context,
        const uint32_t queueFamily,
        const bool transient
    )
        : m_Transient(transient), m_QueueFamily(queueFamily), m_GraphicsContext(context)
    {
        EE_PROFILE_ZONE_SCOPED()
        CreateCommandPool();
    }

    VulkanCommandPool::~VulkanCommandPool()
    {
        EE_PROFILE_ZONE_SCOPED()
        if (m_Pool)
        {
            vkDestroyCommandPool(m_GraphicsContext->GetDevice(), m_Pool, nullptr);
        }
    }

    Ref<CommandBuffer> VulkanCommandPool::GetPrimaryCommandBuffer()
    {
        std::lock_guard lock(m_Mutex);

        // Try to recycle
        const auto frameIndex = m_GraphicsContext->GetFrameIndex();
        auto& commands = m_RecycledCommands[frameIndex];
        for (auto it = commands.begin(); it != commands.end(); ++it)
        {
            if ((*it)->IsPrimary())
            {
                const auto cmd = *it;
                commands.erase(it);
                return cmd;
            }
        }

        // Create a new one
        return CreateCommandBuffer(ECommandBufferLevel::Primary);
    }

    Ref<CommandBuffer> VulkanCommandPool::GetSecondaryCommandBuffer()
    {
        std::lock_guard lock(m_Mutex);

        // Try to recycle
        const auto frameIndex = m_GraphicsContext->GetFrameIndex();
        auto& commands = m_RecycledCommands[frameIndex];
        for (auto it = commands.begin(); it != commands.end(); ++it)
        {
            if ((*it)->IsSecondary())
            {
                const auto cmd = *it;
                commands.erase(it);
                return cmd;
            }
        }

        // Create a new one
        return CreateCommandBuffer(ECommandBufferLevel::Secondary);
    }

    void VulkanCommandPool::Recycle(const std::vector<Ref<CommandBuffer>>& commands)
    {
        std::lock_guard lock(m_Mutex);

        const auto frameIndex = m_GraphicsContext->GetFrameIndex();
        auto& recycled = m_RecycledCommands[frameIndex];

        for (auto& cmd : commands)
        {
            cmd->Reset();
            recycled.push_back(cmd);
        }
    }

    void VulkanCommandPool::CreateCommandPool()
    {
        VkCommandPoolCreateFlags flags = 0;
        if (m_Transient) flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.pNext = nullptr;
        poolInfo.flags = flags;
        poolInfo.queueFamilyIndex = m_QueueFamily;

        VK_CHECK_RESULT(
            vkCreateCommandPool(
                m_GraphicsContext->GetDevice(),
                &poolInfo,
                nullptr,
                &m_Pool
            )
        );

        m_RecycledCommands.resize(m_GraphicsContext->GetFramesInFlight());
    }

    Ref<CommandBuffer> VulkanCommandPool::CreateCommandBuffer(const ECommandBufferLevel level)
    {
        return CreateRef<VulkanCommandBuffer>(m_GraphicsContext, m_Pool, level);
    }

    /* VulkanCommandPoolManager  */

    thread_local VulkanCommandPool* VulkanCommandPoolManager::s_ThreadPool = nullptr;

    VulkanCommandPoolManager::VulkanCommandPoolManager(const VulkanGraphicsContext* context)
        : m_GraphicsContext(context)
    {
        m_GraphicsQueueFamily = m_GraphicsContext->GetGraphicsQueueFamily();
        m_TransferQueueFamily = m_GraphicsContext->GetTransferQueueFamily();
        m_PendingRecycles.resize(m_GraphicsContext->GetFramesInFlight());
    }

    VulkanCommandPoolManager::~VulkanCommandPoolManager()
    {
        std::lock_guard lock(m_MapMutex);
        m_CommandPools.clear();
    }

    VulkanCommandPool* VulkanCommandPoolManager::GetCommandPool()
    {
        if (s_ThreadPool) return s_ThreadPool;

        std::lock_guard lock(m_MapMutex);
        const auto id = std::this_thread::get_id();
        const auto it = m_CommandPools.find(id);
        if (it != m_CommandPools.end())
        {
            s_ThreadPool = it->second.get();
            return s_ThreadPool;
        }

        // Create a new pool for this thread
        auto pool = CreateScope<VulkanCommandPool>(
            m_GraphicsContext,
            m_GraphicsQueueFamily,
            true
        );
        m_CommandPools.emplace(id, std::move(pool));

        s_ThreadPool = m_CommandPools[id].get();
        return m_CommandPools[id].get();
    }

    Ref<CommandBuffer> VulkanCommandPoolManager::GetPrimaryCommandBuffer()
    {
        return GetCommandPool()->GetPrimaryCommandBuffer();
    }

    Ref<CommandBuffer> VulkanCommandPoolManager::GetSecondaryCommandBuffer()
    {
        return GetCommandPool()->GetSecondaryCommandBuffer();
    }

    void VulkanCommandPoolManager::EnqueueSecondaryCommandBuffer(const Ref<CommandBuffer>& cmd)
    {
        std::lock_guard lock(m_CommandsMutex);
        m_CommandsQueue.push(cmd);
    }

    void VulkanCommandPoolManager::FlushSecondaryCommandBuffers(const Ref<CommandBuffer>& cmd)
    {
        std::lock_guard lock(m_CommandsMutex);

        std::vector<Ref<CommandBuffer>> cmds;
        cmds.reserve(m_CommandsQueue.size());

        while (!m_CommandsQueue.empty())
        {
            const auto secondary = m_CommandsQueue.front();
            secondary->End();

            cmds.push_back(secondary);
            m_CommandsQueue.pop();

            RecycleCommandBuffer(secondary);
        }

        cmd->ExecuteCommands(cmds);
    }

    void VulkanCommandPoolManager::RecycleCommandBuffer(const Ref<CommandBuffer>& cmd)
    {
        if (!cmd) return;
        const auto frameIndex = m_GraphicsContext->GetFrameIndex();
        m_PendingRecycles[frameIndex].push_back(cmd);
    }

    void VulkanCommandPoolManager::Recycle()
    {
        std::vector<Ref<CommandBuffer>> used;

        {
            std::lock_guard lock(m_CommandsMutex);
            const auto frameIndex = m_GraphicsContext->GetFrameIndex();
            used.swap(m_PendingRecycles[frameIndex]);
        }

        if (used.empty()) return;

        auto lookup = CollectUsedForRecycle(used);
        for (auto& [pool, cmds] : lookup)
            pool->Recycle(cmds);
    }

    CommandBufferLookup VulkanCommandPoolManager::CollectUsedForRecycle(
        const std::vector<Ref<CommandBuffer>>& used
    )
    {
        std::vector<std::pair<VulkanCommandPool*, std::vector<Ref<CommandBuffer>>>> groups;
        std::unordered_map<VulkanCommandPool*, std::vector<Ref<CommandBuffer>>> lookup;

        {
            std::lock_guard cmdLock(m_CommandsMutex);

            for (auto& cmd : used)
            {
                const auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
                VulkanCommandPool* pool = nullptr;

                {
                    std::lock_guard mapLock(m_MapMutex);
                    for (auto& p : m_CommandPools | std::views::values)
                    {
                        if (p->GetVulkanCommandPool() == vkCmd->GetVulkanCommandPool())
                        {
                            pool = p.get();
                            break;
                        }
                    }
                }

                if (pool)
                {
                    lookup[pool].push_back(cmd);
                }
                else
                {
                    if (m_UploadPool && m_UploadPool->GetVulkanCommandPool() == vkCmd->GetVulkanCommandPool())
                    {
                        const auto uploadPool = m_UploadPool.get();
                        lookup[uploadPool].push_back(cmd);
                    }
                    else
                    {
                        // Unknown pool - push to a default recycling bucket
                    }
                }
            }
        }

        for (auto& [pool, cmds] : lookup)
            groups.emplace_back(pool, cmds);

        return groups;
    }
}