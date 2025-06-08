#include "epch.h"
#include "VulkanCommandBuffer.h"

#include "VulkanGraphicsContext.h"
#include "Converters.h"
#include "Utils.h"

namespace Elixir::Vulkan
{
    VulkanCommandBuffer::VulkanCommandBuffer(GraphicsContext* context)
        : m_Ended(false)
    {
        EE_PROFILE_ZONE_SCOPED()

        m_GraphicsContext = static_cast<VulkanGraphicsContext*>(context);

        // TODO: Should use centralized managed command pools!
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.pNext = nullptr;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_GraphicsContext->GetGraphicsQueueFamily();

        auto err = vkCreateCommandPool(
            m_GraphicsContext->GetDevice(),
            &poolInfo,
            nullptr,
            &m_CommandPool
        );
        EE_CORE_ASSERT(err == VK_SUCCESS, "Failed to create command pool!")

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.commandBufferCount = 1;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        err = vkAllocateCommandBuffers(
            m_GraphicsContext->GetDevice(),
            &allocInfo,
            &m_CommandBuffer
        );
        EE_CORE_ASSERT(err == VK_SUCCESS, "Failed to allocate command buffer!")
    }

    VulkanCommandBuffer::~VulkanCommandBuffer()
    {
        EE_PROFILE_ZONE_SCOPED()

        // TODO: Handle this in a command pool manager!
        vkDestroyCommandPool(m_GraphicsContext->GetDevice(), m_CommandPool, nullptr);
    }

    void VulkanCommandBuffer::Begin()
    {
        EE_PROFILE_ZONE_SCOPED()

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.pInheritanceInfo = nullptr;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK_RESULT(vkBeginCommandBuffer(m_CommandBuffer, &beginInfo));
        m_Ended = false;
    }

    void VulkanCommandBuffer::End()
    {
        EE_PROFILE_ZONE_SCOPED()

        if (m_Ended) return;

        vkEndCommandBuffer(m_CommandBuffer);
        m_Ended = true;
    }

    void VulkanCommandBuffer::EndRendering()
    {
        EE_PROFILE_ZONE_SCOPED()
        vkCmdEndRendering(m_CommandBuffer);
    }

    void VulkanCommandBuffer::Draw(
        const uint32_t vertexCount,
        const uint32_t instanceCount,
        const uint32_t firstVertex,
        const uint32_t firstInstance
    )
    {
        EE_PROFILE_ZONE_SCOPED()
        vkCmdDraw(m_CommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void VulkanCommandBuffer::DrawIndexed(
        const uint32_t indexCount,
        const uint32_t instanceCount,
        const uint32_t firstIndex,
        const uint32_t vertexOffset,
        const uint32_t firstInstance
    )
    {
        EE_PROFILE_ZONE_SCOPED()
        vkCmdDrawIndexed(
            m_CommandBuffer,
            indexCount,
            instanceCount,
            firstIndex,
            vertexOffset,
            firstInstance
        );
    }

    void VulkanCommandBuffer::SetViewports(
        const std::vector<Viewport>& viewports,
        const uint32_t firstViewport
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        std::vector<VkViewport> data(viewports.size());

        std::ranges::transform(
            viewports,
            data.begin(),
            Converters::GetViewport
        );

        vkCmdSetViewport(
            m_CommandBuffer,
            firstViewport,
            (uint32_t)data.size(),
            data.data()
        );
    }

    void VulkanCommandBuffer::SetScissors(
        const std::vector<Rect2D>& scissors,
        const uint32_t firstScissor
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        std::vector<VkRect2D> data(scissors.size());

        std::ranges::transform(
            scissors,
            data.begin(),
            Converters::GetRect2D
        );

        vkCmdSetScissor(
            m_CommandBuffer,
            firstScissor,
            (uint32_t)data.size(),
            data.data()
        );
    }

    void VulkanCommandBuffer::Flush()
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto device = m_GraphicsContext->GetDevice();
        const auto graphicsQueue = m_GraphicsContext->GetGraphicsQueue();

        End();

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_CommandBuffer;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        VkFence fence;
        VK_CHECK_RESULT(vkCreateFence(device, &fenceInfo, nullptr, &fence));

        VK_CHECK_RESULT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence));

        VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
        vkDestroyFence(device, fence, nullptr);
    }

    void VulkanCommandBuffer::Submit(
        const VkSemaphore swapchainSemaphore,
        const VkSemaphore renderSemaphore,
        const VkFence renderFence
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto graphicsQueue = m_GraphicsContext->GetGraphicsQueue();

        End();

        VkCommandBufferSubmitInfo cmdInfo = {};
        cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cmdInfo.pNext = nullptr;
        cmdInfo.commandBuffer = m_CommandBuffer;
        cmdInfo.deviceMask = 0;

        VkSemaphoreSubmitInfo waitInfo = {};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitInfo.pNext = nullptr;
        waitInfo.semaphore = swapchainSemaphore;
        waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
        waitInfo.deviceIndex = 0;
        waitInfo.value = 1;

        VkSemaphoreSubmitInfo signalInfo = {};
        signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalInfo.pNext = nullptr;
        signalInfo.semaphore = renderSemaphore;
        signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;  // NOTE: Possible optimization.
        signalInfo.deviceIndex = 0;
        signalInfo.value = 1;

        VkSubmitInfo2 submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.pNext = nullptr;
        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &waitInfo;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &signalInfo;
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &cmdInfo;

        VK_CHECK_RESULT(vkQueueSubmit2(graphicsQueue, 1, &submitInfo, renderFence));
    }
}