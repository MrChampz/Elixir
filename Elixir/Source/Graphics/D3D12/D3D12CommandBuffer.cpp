#include "epch.h"
#include "D3D12CommandBuffer.h"

#ifdef EE_PLATFORM_WINDOWS

#include <Graphics/D3D12/D3D12Buffer.h>
#include <Graphics/D3D12/D3D12Image.h>
#include <Graphics/D3D12/D3D12Pipeline.h>
#include <Graphics/D3D12/D3D12Shader.h>

#include <iterator>

namespace Elixir::D3D12
{
    D3D12CommandBuffer::D3D12CommandBuffer(
        const GraphicsContext* context,
        const ECommandBufferLevel level
    ) : CommandBuffer(context, level)
    {
        m_GraphicsContext = static_cast<const D3D12GraphicsContext*>(context);
        CreateCommandObjects();
    }

    void D3D12CommandBuffer::Begin(const SRenderingInfo& info)
    {
        if (!m_Ended)
            return;

        m_RenderingInfo = info;
        Reset();
        m_Ended = false;
    }

    void D3D12CommandBuffer::End()
    {
        if (m_Ended)
            return;

        DX_CHECK_RESULT(m_CommandList->Close());
        m_Ended = true;
    }

    void D3D12CommandBuffer::Reset()
    {
        DX_CHECK_RESULT(m_CommandAllocator->Reset());
        DX_CHECK_RESULT(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));
        m_CurrentGraphicsPipeline = nullptr;
    }

    void D3D12CommandBuffer::Dispatch(
        const uint32_t groupCountX,
        const uint32_t groupCountY,
        const uint32_t groupCountZ
    )
    {
        m_CommandList->Dispatch(groupCountX, groupCountY, groupCountZ);
    }

    void D3D12CommandBuffer::BeginRendering(const SRenderingInfo& info)
    {
        Begin(info);

        if (info.ColorAttachment)
            info.ColorAttachment->Transition(this, EImageLayout::ColorAttachment);

        if (info.DepthStencilAttachment)
            info.DepthStencilAttachment->Transition(this, EImageLayout::DepthStencilAttachment);

        const auto colorImage = dynamic_cast<D3D12BaseImage<Texture2D>*>(info.ColorAttachment.get());
        const auto fallbackImage = dynamic_cast<D3D12BaseImage<Image>*>(info.ColorAttachment.get());

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = {};
        if (colorImage)
            rtv = colorImage->GetRenderTargetView();
        else if (fallbackImage)
            rtv = fallbackImage->GetRenderTargetView();

        D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
        if (info.DepthStencilAttachment)
        {
            const auto depth = dynamic_cast<D3D12BaseImage<DepthStencilImage>*>(
                info.DepthStencilAttachment.get()
            );
            if (depth)
                dsv = depth->GetDepthStencilView();
        }

        m_CommandList->OMSetRenderTargets(1, &rtv, FALSE, info.DepthStencilAttachment ? &dsv : nullptr);

        if (info.DepthStencilAttachment)
        {
            m_CommandList->ClearDepthStencilView(
                dsv,
                D3D12_CLEAR_FLAG_DEPTH,
                info.DepthClearValue,
                0,
                0,
                nullptr
            );
        }
    }

    void D3D12CommandBuffer::EndRendering()
    {
        // Direct3D 12 has no dynamic-rendering end marker for this path.
    }

    void D3D12CommandBuffer::Draw(
        const uint32_t vertexCount,
        const uint32_t instanceCount,
        const uint32_t firstVertex,
        const uint32_t firstInstance
    )
    {
        m_CommandList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void D3D12CommandBuffer::DrawIndexed(
        const uint32_t indexCount,
        const uint32_t instanceCount,
        const uint32_t firstIndex,
        const uint32_t vertexOffset,
        const uint32_t firstInstance
    )
    {
        m_CommandList->DrawIndexedInstanced(
            indexCount,
            instanceCount,
            firstIndex,
            vertexOffset,
            firstInstance
        );
    }

    void D3D12CommandBuffer::SetViewports(
        const std::vector<Viewport>& viewports,
        const uint32_t firstViewport
    )
    {
        std::vector<D3D12_VIEWPORT> data;
        data.reserve(viewports.size());
        std::ranges::transform(viewports, std::back_inserter(data), Converters::GetViewport);
        m_CommandList->RSSetViewports((UINT)data.size() - firstViewport, data.data() + firstViewport);
    }

    void D3D12CommandBuffer::SetScissors(
        const std::vector<Rect2D>& scissors,
        const uint32_t firstScissor
    )
    {
        std::vector<D3D12_RECT> data;
        data.reserve(scissors.size());
        std::ranges::transform(scissors, std::back_inserter(data), Converters::GetRect);
        m_CommandList->RSSetScissorRects((UINT)data.size() - firstScissor, data.data() + firstScissor);
    }

    void D3D12CommandBuffer::SetPushConstant(
        const Ref<PushConstantBuffer>& buffer,
        const Shader*,
        const EShaderStage stages
    )
    {
        const auto dwords = std::min<UINT>(
            D3D12Shader::MaxPushConstantDWords,
            (buffer->GetSize() + sizeof(uint32_t) - 1) / sizeof(uint32_t)
        );

        if (stages & EShaderStage::Compute)
        {
            m_CommandList->SetComputeRoot32BitConstants(
                D3D12Shader::RootPushConstants,
                dwords,
                buffer->GetBuffer().Data,
                0
            );
        }
        else
        {
            m_CommandList->SetGraphicsRoot32BitConstants(
                D3D12Shader::RootPushConstants,
                dwords,
                buffer->GetBuffer().Data,
                0
            );
        }
    }

    void D3D12CommandBuffer::BindPipeline(const GraphicsPipeline* pipeline)
    {
        const auto d3dPipeline = static_cast<const D3D12GraphicsPipeline*>(pipeline);
        m_CurrentGraphicsPipeline = d3dPipeline;
        m_CommandList->SetGraphicsRootSignature(d3dPipeline->GetRootSignature());
        m_CommandList->SetPipelineState(d3dPipeline->GetPipelineState());
        m_CommandList->IASetPrimitiveTopology(d3dPipeline->GetPrimitiveTopology());
    }

    void D3D12CommandBuffer::BindPipeline(const ComputePipeline* pipeline)
    {
        const auto d3dPipeline = static_cast<const D3D12ComputePipeline*>(pipeline);
        m_CommandList->SetComputeRootSignature(d3dPipeline->GetRootSignature());
        m_CommandList->SetPipelineState(d3dPipeline->GetPipelineState());
    }

    void D3D12CommandBuffer::BindVertexBuffers(
        const std::span<const VertexBuffer*> vertexBuffers,
        std::span<uint64_t> offsets,
        const uint32_t bindingCount,
        const uint32_t firstBinding
    )
    {
        std::vector<D3D12_VERTEX_BUFFER_VIEW> views;
        views.reserve(vertexBuffers.size());

        for (auto i = 0u; i < vertexBuffers.size(); ++i)
        {
            const auto d3dBuffer = dynamic_cast<const D3D12BaseBuffer<VertexBuffer>*>(vertexBuffers[i]);
            const auto offset = offsets.empty() ? 0 : offsets[i];
            views.push_back(d3dBuffer->GetVertexBufferView(offset));
        }

        m_CommandList->IASetVertexBuffers(firstBinding, bindingCount, views.data());
    }

    void D3D12CommandBuffer::BindVertexBuffers(
        const std::span<const DynamicVertexBuffer*> vertexBuffers,
        std::span<uint64_t> offsets,
        const uint32_t bindingCount,
        const uint32_t firstBinding
    )
    {
        std::vector<D3D12_VERTEX_BUFFER_VIEW> views;
        views.reserve(vertexBuffers.size());

        for (auto i = 0u; i < vertexBuffers.size(); ++i)
        {
            const auto d3dBuffer = dynamic_cast<const D3D12DynamicBuffer<DynamicVertexBuffer>*>(vertexBuffers[i]);
            const auto offset = offsets.empty() ? 0 : offsets[i];
            views.push_back(d3dBuffer->GetVertexBufferView(offset));
        }

        m_CommandList->IASetVertexBuffers(firstBinding, bindingCount, views.data());
    }

    void D3D12CommandBuffer::BindVertexBuffers(
        const std::span<const Buffer*> vertexBuffers,
        std::span<uint64_t> offsets,
        const uint32_t bindingCount,
        const uint32_t firstBinding
    )
    {
        EE_CORE_ASSERT(m_CurrentGraphicsPipeline, "A graphics pipeline must be bound before generic vertex buffers!")

        std::vector<D3D12_VERTEX_BUFFER_VIEW> views;
        views.reserve(vertexBuffers.size());

        const auto& layout = m_CurrentGraphicsPipeline->GetBufferLayout();

        for (auto i = 0u; i < vertexBuffers.size(); ++i)
        {
            const auto binding = layout.GetBinding(firstBinding + i);
            const auto offset = offsets.empty() ? 0 : offsets[i];
            const auto size = vertexBuffers[i]->GetSize() - offset;

            views.push_back({
                .BufferLocation = TryToGetD3D12Buffer(vertexBuffers[i])->GetGPUVirtualAddress() + offset,
                .SizeInBytes = (UINT)size,
                .StrideInBytes = (UINT)(binding ? binding->GetStride() : 0),
            });
        }

        m_CommandList->IASetVertexBuffers(firstBinding, bindingCount, views.data());
    }

    void D3D12CommandBuffer::BindIndexBuffer(const IndexBuffer* indexBuffer)
    {
        const auto d3dBuffer = dynamic_cast<const D3D12BaseBuffer<IndexBuffer>*>(indexBuffer);
        const auto view = d3dBuffer->GetIndexBufferView(indexBuffer->GetIndexType());
        m_CommandList->IASetIndexBuffer(&view);
    }

    void D3D12CommandBuffer::BindIndexBuffer(const DynamicIndexBuffer* indexBuffer)
    {
        const auto d3dBuffer = dynamic_cast<const D3D12DynamicBuffer<DynamicIndexBuffer>*>(indexBuffer);
        const auto view = d3dBuffer->GetIndexBufferView(indexBuffer->GetIndexType());
        m_CommandList->IASetIndexBuffer(&view);
    }

    void D3D12CommandBuffer::BindIndexBuffer(
        const Buffer* indexBuffer,
        const EIndexType indexType
    )
    {
        const D3D12_INDEX_BUFFER_VIEW view = {
            .BufferLocation = TryToGetD3D12Buffer(indexBuffer)->GetGPUVirtualAddress(),
            .SizeInBytes = (UINT)indexBuffer->GetSize(),
            .Format = Converters::GetIndexFormat(indexType),
        };
        m_CommandList->IASetIndexBuffer(&view);
    }

    void D3D12CommandBuffer::ExecuteCommands(const std::span<Ref<CommandBuffer>>)
    {
        // Secondary command lists are executed by D3D12GraphicsContext::Submit.
    }

    void D3D12CommandBuffer::Flush()
    {
        End();
        ID3D12CommandList* lists[] = { m_CommandList.Get() };
        m_GraphicsContext->ExecuteCommandListsAndWait(lists);
    }

    void D3D12CommandBuffer::CreateCommandObjects()
    {
        DX_CHECK_RESULT(
            m_GraphicsContext->GetDevice()->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&m_CommandAllocator)
            )
        );

        DX_CHECK_RESULT(
            m_GraphicsContext->GetDevice()->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                m_CommandAllocator.Get(),
                nullptr,
                IID_PPV_ARGS(&m_CommandList)
            )
        );

        DX_CHECK_RESULT(m_CommandList->Close());
    }
}

#endif
