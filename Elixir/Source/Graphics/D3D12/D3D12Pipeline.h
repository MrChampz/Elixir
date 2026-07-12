#pragma once

#ifdef EE_PLATFORM_WINDOWS

#include <Engine/Graphics/Pipeline/Pipeline.h>
#include <Graphics/D3D12/D3D12GraphicsContext.h>

namespace Elixir::D3D12
{
    class ELIXIR_API D3D12GraphicsPipeline final : public GraphicsPipeline
    {
      public:
        D3D12GraphicsPipeline(const GraphicsContext* context, const SPipelineCreateInfo& info);
        ~D3D12GraphicsPipeline() override = default;

        void Bind(const Ref<CommandBuffer>& cmd) override;

        ID3D12PipelineState* GetPipelineState() const { return m_PipelineState.Get(); }
        ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }
        D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveTopology() const { return m_PrimitiveTopology; }

      private:
        void CreatePipeline();
        void CreateInputLayout();

        ComPtr<ID3D12PipelineState> m_PipelineState;
        ComPtr<ID3D12RootSignature> m_RootSignature;
        D3D12_PRIMITIVE_TOPOLOGY m_PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

        std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputElements;
        std::vector<std::string> m_InputSemanticNames;

        const D3D12GraphicsContext* m_D3D12Context = nullptr;
    };

    class ELIXIR_API D3D12ComputePipeline final : public ComputePipeline
    {
      public:
        D3D12ComputePipeline(const GraphicsContext* context, const SPipelineCreateInfo& info);
        ~D3D12ComputePipeline() override = default;

        void Bind(const Ref<CommandBuffer>& cmd) override;

        ID3D12PipelineState* GetPipelineState() const { return m_PipelineState.Get(); }
        ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }

      private:
        void CreatePipeline();

        ComPtr<ID3D12PipelineState> m_PipelineState;
        ComPtr<ID3D12RootSignature> m_RootSignature;
        const D3D12GraphicsContext* m_D3D12Context = nullptr;
    };
}

#endif
