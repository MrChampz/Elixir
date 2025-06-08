#pragma once

#include <Engine/Graphics/CommandBuffer.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

#include <vulkan/vulkan.h>

namespace Elixir::Vulkan
{
    class ELIXIR_API VulkanCommandBuffer final : public CommandBuffer
    {
        friend class VulkanGraphicsContext;

      public:
        explicit VulkanCommandBuffer(GraphicsContext* context);
        ~VulkanCommandBuffer() override;

        void Begin() override;
        void End() override;

        void EndRendering() override;

        void Draw(
            uint32_t vertexCount,
            uint32_t instanceCount,
            uint32_t firstVertex,
            uint32_t firstInstance
        ) override;

        void DrawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount,
            uint32_t firstIndex,
            uint32_t vertexOffset,
            uint32_t firstInstance
        ) override;

        void SetViewports(const std::vector<Viewport>& viewports, uint32_t firstViewport) override;
        void SetScissors(const std::vector<Rect2D>& scissors, uint32_t firstScissor) override;

        void Flush() override;

        VkCommandBuffer GetVulkanCommandBuffer() const { return m_CommandBuffer; }

      protected:
        void Submit(VkSemaphore swapchainSemaphore, VkSemaphore renderSemaphore, VkFence renderFence);

        // VkBufferImageCopy GetDefaultBufferImageCopy(const ) const;

      private:
        bool m_Ended;

        VkCommandPool m_CommandPool;
        VkCommandBuffer m_CommandBuffer;

        VulkanGraphicsContext* m_GraphicsContext;
    };
}
