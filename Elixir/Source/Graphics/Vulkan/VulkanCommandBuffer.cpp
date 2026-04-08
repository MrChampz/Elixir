#include "epch.h"
#include "VulkanCommandBuffer.h"

#include "VulkanGraphicsContext.h"
#include "Converters.h"
#include "Utils.h"
#include "VulkanBuffer.h"
#include "VulkanPipeline.h"

namespace Elixir::Vulkan
{
    VulkanCommandBuffer::VulkanCommandBuffer(
        const GraphicsContext* context,
        const VkCommandPool pool,
        const ECommandBufferLevel level
    ) : CommandBuffer(context, level), m_Ended(true), m_CommandPool(pool),
        m_InheritanceInfo(nullptr)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);
        AllocateCommandBuffer();
    }

    VulkanCommandBuffer::~VulkanCommandBuffer()
    {
        EE_PROFILE_ZONE_SCOPED()

        if (m_InheritanceInfo)
        {
            delete m_InheritanceInfo;
            m_InheritanceInfo = nullptr;
        }
    }

    void VulkanCommandBuffer::Begin(const SRenderingInfo& info)
    {
        EE_PROFILE_ZONE_SCOPED()
        if (!m_Ended) return;

        m_RenderingInfo = info;

        if (IsSecondary())
        {
            if (info.ColorAttachment)
            {
                const auto colorFormat = Converters::GetFormat(info.ColorAttachment->GetFormat());
                const auto depthFormat =
                    info.DepthStencilAttachment
                        ? Converters::GetFormat(info.DepthStencilAttachment->GetFormat())
                        : VK_FORMAT_UNDEFINED;

                VkCommandBufferInheritanceRenderingInfo renderingInfo = {};
                renderingInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO;
                renderingInfo.colorAttachmentCount = 1;
                renderingInfo.pColorAttachmentFormats = &colorFormat;
                renderingInfo.depthAttachmentFormat = depthFormat;
                renderingInfo.viewMask = 0;
                renderingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

                m_InheritanceInfo = new VkCommandBufferInheritanceInfo();
                m_InheritanceInfo->sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
                m_InheritanceInfo->pNext = &renderingInfo;
            }
            else
            {
                EE_CORE_ASSERT(false, "Secondary command buffers must have at least one color attachment!")
            }
        }

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.pInheritanceInfo = m_InheritanceInfo;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK_RESULT(vkBeginCommandBuffer(m_CommandBuffer, &beginInfo));
        m_Ended = false;
    }

    void VulkanCommandBuffer::End()
    {
        EE_PROFILE_ZONE_SCOPED()

        if (m_Ended) return;

        VK_CHECK_RESULT(vkEndCommandBuffer(m_CommandBuffer));
        m_Ended = true;
    }

    void VulkanCommandBuffer::Reset()
    {
        EE_PROFILE_ZONE_SCOPED()
        VK_CHECK_RESULT(vkResetCommandBuffer(m_CommandBuffer, 0));
    }

    void VulkanCommandBuffer::Dispatch(
        const uint32_t groupCountX,
        const uint32_t groupCountY,
        const uint32_t groupCountZ
    )
    {
        EE_PROFILE_ZONE_SCOPED()
        vkCmdDispatch(m_CommandBuffer, groupCountX, groupCountY, groupCountZ);
    }

    void VulkanCommandBuffer::BeginRendering(const SRenderingInfo& info)
    {
        EE_PROFILE_ZONE_SCOPED()

        m_RenderingInfo = info;
        Begin(info);

        const auto colorInfo = Initializers::AttachmentInfo(info.ColorAttachment);

        const VkRenderingAttachmentInfo* depthStencilInfo = nullptr;
        if (info.DepthStencilAttachment)
        {
            const auto attachment = Initializers::DepthStencilAttachmentInfo(
                info.DepthStencilAttachment
            );
            depthStencilInfo = &attachment;
        }

        const auto renderInfo = Initializers::RenderingInfo(
            info.RenderArea,
            &colorInfo,
            depthStencilInfo
        );

        vkCmdBeginRendering(m_CommandBuffer, &renderInfo);
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

    void VulkanCommandBuffer::BindPipeline(const GraphicsPipeline* pipeline)
    {
        const auto vkPipeline = static_cast<const VulkanGraphicsPipeline*>(pipeline);
        vkCmdBindPipeline(
            m_CommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            vkPipeline->GetVulkanPipeline()
        );
    }

    void VulkanCommandBuffer::BindPipeline(const ComputePipeline* pipeline)
    {
        const auto vkPipeline = static_cast<const VulkanComputePipeline*>(pipeline);
        vkCmdBindPipeline(
            m_CommandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            vkPipeline->GetVulkanPipeline()
        );
    }

    void VulkanCommandBuffer::BindVertexBuffers(
        const std::span<const VertexBuffer*> vertexBuffers,
        std::span<uint64_t> offsets,
        const uint32_t bindingCount,
        const uint32_t firstBinding
    )
    {
        std::vector<VkBuffer> buffers;
        buffers.reserve(vertexBuffers.size());

        for (const auto& buffer : vertexBuffers)
        {
            const auto vkBuffer = static_cast<const VulkanVertexBuffer*>(buffer);
            buffers.push_back(vkBuffer->GetVulkanBuffer());
        }

        if (offsets.size() == 0)
        {
            std::array<uint64_t, 1> defaultOffset = { 0 };
            offsets = defaultOffset;
        }

        vkCmdBindVertexBuffers(
            m_CommandBuffer,
            firstBinding,
            bindingCount,
            buffers.data(),
            offsets.data()
        );
    }

    void VulkanCommandBuffer::BindVertexBuffers(
        const std::span<const DynamicVertexBuffer*> vertexBuffers,
        std::span<uint64_t> offsets,
        const uint32_t bindingCount,
        const uint32_t firstBinding
    )
    {
        std::vector<VkBuffer> buffers;
        buffers.reserve(vertexBuffers.size());

        for (const auto& buffer : vertexBuffers)
        {
            const auto vkBuffer = static_cast<const VulkanDynamicVertexBuffer*>(buffer);
            buffers.push_back(vkBuffer->GetVulkanBuffer());
        }

        if (offsets.size() == 0)
        {
            std::array<uint64_t, 1> defaultOffset = { 0 };
            offsets = defaultOffset;
        }

        vkCmdBindVertexBuffers(
            m_CommandBuffer,
            firstBinding,
            bindingCount,
            buffers.data(),
            offsets.data()
        );
    }

    void VulkanCommandBuffer::BindVertexBuffers(
        const std::span<const StorageBuffer*> storageBuffers,
        std::span<uint64_t> offsets,
        const uint32_t bindingCount,
        const uint32_t firstBinding
    )
    {
        std::vector<VkBuffer> buffers;
        buffers.reserve(storageBuffers.size());

        for (const auto& buffer : storageBuffers)
        {
            const auto vkBuffer = static_cast<const VulkanStorageBuffer*>(buffer);
            buffers.push_back(vkBuffer->GetVulkanBuffer());
        }

        if (offsets.size() == 0)
        {
            std::array<uint64_t, 1> defaultOffset = { 0 };
            offsets = defaultOffset;
        }

        vkCmdBindVertexBuffers(
            m_CommandBuffer,
            firstBinding,
            bindingCount,
            buffers.data(),
            offsets.data()
        );
    }

    void VulkanCommandBuffer::BindIndexBuffer(const IndexBuffer* indexBuffer)
    {
        const auto vkBuffer = static_cast<const VulkanIndexBuffer*>(indexBuffer);
        vkCmdBindIndexBuffer(
            m_CommandBuffer,
            vkBuffer->GetVulkanBuffer(),
            0,
            Converters::GetIndexType(indexBuffer->GetIndexType())
        );
    }

    void VulkanCommandBuffer::BindIndexBuffer(const DynamicIndexBuffer* indexBuffer)
    {
        const auto vkBuffer = static_cast<const VulkanDynamicIndexBuffer*>(indexBuffer);
        vkCmdBindIndexBuffer(
            m_CommandBuffer,
            vkBuffer->GetVulkanBuffer(),
            0,
            Converters::GetIndexType(indexBuffer->GetIndexType())
        );
    }

    void VulkanCommandBuffer::BindDescriptorSets(
        const Pipeline* pipeline,
        const VkPipelineLayout layout,
        const uint32_t firstSet,
        const std::vector<VkDescriptorSet>& descriptorSets,
        const uint32_t dynamicOffsetCount,
        const uint32_t* dynamicOffsets
    ) const
    {
        if (descriptorSets.empty()) return;

        vkCmdBindDescriptorSets(
            m_CommandBuffer,
            pipeline->IsGraphics()
                ? VK_PIPELINE_BIND_POINT_GRAPHICS
                : VK_PIPELINE_BIND_POINT_COMPUTE,
            layout,
            firstSet,
            descriptorSets.size(),
            descriptorSets.data(),
            dynamicOffsetCount,
            dynamicOffsets
        );
    }

    void VulkanCommandBuffer::ExecuteCommands(const std::span<Ref<CommandBuffer>> cmds)
    {
        if (cmds.empty()) return;

        std::vector<VkCommandBuffer> vkCmds;
        vkCmds.reserve(cmds.size());

        for (auto const& cmd : cmds)
        {
            const auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
            vkCmds.push_back(vkCmd->GetVulkanCommandBuffer());
        }

        vkCmdExecuteCommands(
            m_CommandBuffer,
            (uint32_t)vkCmds.size(),
            vkCmds.data()
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

    void VulkanCommandBuffer::AllocateCommandBuffer()
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.commandBufferCount = 1;
        allocInfo.level = Converters::GetCommandBufferLevel(m_Level);

        VK_CHECK_RESULT(
            vkAllocateCommandBuffers(
                m_GraphicsContext->GetDevice(),
                &allocInfo,
                &m_CommandBuffer
            )
        );
    }

    void VulkanCommandBuffer::Submit(
        const VkSemaphore swapchainSemaphore,
        const VkSemaphore renderSemaphore,
        const VkFence renderFence
    )
    {
        EE_PROFILE_ZONE_SCOPED()
        EE_CORE_ASSERT(!IsSecondary(), "Secondary command buffers cannot be submitted!")

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
        waitInfo.value = 0;

        VkSemaphoreSubmitInfo signalInfo = {};
        signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalInfo.pNext = nullptr;
        signalInfo.semaphore = renderSemaphore;
        signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;  // NOTE: Possible optimization.
        signalInfo.deviceIndex = 0;
        signalInfo.value = 0;

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