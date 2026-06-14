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
        explicit VulkanCommandBuffer(
            const GraphicsContext* context,
            VkCommandPool pool,
            ECommandBufferLevel level
        );
        ~VulkanCommandBuffer() override;

        void Begin(const SRenderingInfo& info = {}) override;
        void End() override;

        void Reset() override;

        /** Dispatching compute methods */

        void Dispatch(
            uint32_t groupCountX,
            uint32_t groupCountY,
            uint32_t groupCountZ
        ) override;

        /** Drawing methods **/

        void BeginRendering(const SRenderingInfo& info) override;
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

        /** Set methods **/

        void SetViewports(const std::vector<Viewport>& viewports, uint32_t firstViewport) override;
        void SetScissors(const std::vector<Rect2D>& scissors, uint32_t firstScissor) override;

        void SetPushConstant(
            const Ref<PushConstantBuffer>& buffer,
            const Shader* shader,
            EShaderStage stages
        ) override;

        /** Bind methods **/

        void BindPipeline(const GraphicsPipeline* pipeline) override;
        void BindPipeline(const ComputePipeline* pipeline) override;

        void BindVertexBuffers(
            std::span<const VertexBuffer*> vertexBuffers,
            std::span<uint64_t> offsets,
            uint32_t bindingCount,
            uint32_t firstBinding
        ) override;
        void BindVertexBuffers(
            std::span<const DynamicVertexBuffer*> vertexBuffers,
            std::span<uint64_t> offsets,
            uint32_t bindingCount,
            uint32_t firstBinding
        ) override;
        void BindVertexBuffers(
            std::span<const Buffer*> vertexBuffers,
            std::span<uint64_t> offsets,
            uint32_t bindingCount,
            uint32_t firstBinding
        ) override;
        void BindIndexBuffer(const IndexBuffer* indexBuffer) override;
        void BindIndexBuffer(const DynamicIndexBuffer* indexBuffer) override;
        void BindIndexBuffer(const Buffer* indexBuffer, EIndexType indexType) override;

        void BindDescriptorSets(
            const Pipeline* pipeline,
            VkPipelineLayout layout,
            uint32_t firstSet,
            const std::vector<VkDescriptorSet>& descriptorSets,
            uint32_t dynamicOffsetCount = 0,
            const uint32_t* dynamicOffsets = nullptr
        ) const;

        /** Submit and execution methods **/

        void ExecuteCommands(std::span<Ref<CommandBuffer>> cmds) override;

        void Flush() override;

        /** Getters and Setters **/

        VkCommandBuffer GetVulkanCommandBuffer() const { return m_CommandBuffer; }
        VkCommandPool GetVulkanCommandPool() const { return m_CommandPool; }

      protected:
        void AllocateCommandBuffer();
        void Submit(VkSemaphore swapchainSemaphore, VkSemaphore renderSemaphore, VkFence renderFence);

      private:
        bool m_Ended;
        SRenderingInfo m_RenderingInfo;

        VkCommandPool m_CommandPool;
        VkCommandBuffer m_CommandBuffer;

        VkCommandBufferInheritanceInfo* m_InheritanceInfo;

        const VulkanGraphicsContext* m_GraphicsContext;
    };
}
