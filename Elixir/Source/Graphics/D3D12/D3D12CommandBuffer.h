#pragma once

#ifdef EE_PLATFORM_WINDOWS

#include <Engine/Graphics/CommandBuffer.h>
#include <Graphics/D3D12/D3D12GraphicsContext.h>

namespace Elixir::D3D12
{
    class D3D12GraphicsPipeline;

    class ELIXIR_API D3D12CommandBuffer final : public CommandBuffer
    {
      public:
        explicit D3D12CommandBuffer(
            const GraphicsContext* context,
            ECommandBufferLevel level
        );
        ~D3D12CommandBuffer() override = default;

        void Begin(const SRenderingInfo& info = {}) override;
        void End() override;
        void Reset() override;

        void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

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

        void SetViewports(const std::vector<Viewport>& viewports, uint32_t firstViewport) override;
        void SetScissors(const std::vector<Rect2D>& scissors, uint32_t firstScissor) override;

        void SetPushConstant(
            const Ref<PushConstantBuffer>& buffer,
            const Shader* shader,
            EShaderStage stages
        ) override;

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

        void ExecuteCommands(std::span<Ref<CommandBuffer>> cmds) override;
        void Flush() override;

        ID3D12GraphicsCommandList* GetD3D12CommandList() const { return m_CommandList.Get(); }

      private:
        void CreateCommandObjects();

        bool m_Ended = true;
        SRenderingInfo m_RenderingInfo;

        ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
        ComPtr<ID3D12GraphicsCommandList> m_CommandList;

        const D3D12GraphicsContext* m_GraphicsContext = nullptr;
        const D3D12GraphicsPipeline* m_CurrentGraphicsPipeline = nullptr;
    };
}

#endif
