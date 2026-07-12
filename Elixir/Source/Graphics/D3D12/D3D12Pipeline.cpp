#include "epch.h"
#include "D3D12Pipeline.h"

#ifdef EE_PLATFORM_WINDOWS

#include <Graphics/D3D12/D3D12CommandBuffer.h>
#include <Graphics/D3D12/D3D12Shader.h>
#include <Graphics/D3D12/D3D12ShaderModule.h>

namespace Elixir::D3D12
{
    namespace
    {
        D3D12_RENDER_TARGET_BLEND_DESC GetBlendAttachment(const SColorBlendAttachment& attachment)
        {
            D3D12_RENDER_TARGET_BLEND_DESC desc = {};
            desc.BlendEnable = attachment.BlendEnable;
            desc.LogicOpEnable = FALSE;
            desc.SrcBlend = Converters::GetBlend(attachment.SrcColorBlendFactor);
            desc.DestBlend = Converters::GetBlend(attachment.DstColorBlendFactor);
            desc.BlendOp = Converters::GetBlendOp(attachment.ColorBlendOp);
            desc.SrcBlendAlpha = Converters::GetBlend(attachment.SrcAlphaBlendFactor);
            desc.DestBlendAlpha = Converters::GetBlend(attachment.DstAlphaBlendFactor);
            desc.BlendOpAlpha = Converters::GetBlendOp(attachment.AlphaBlendOp);

            if (desc.BlendOp == D3D12_BLEND_OP_MIN || desc.BlendOp == D3D12_BLEND_OP_MAX)
            {
                desc.SrcBlend = D3D12_BLEND_ONE;
                desc.DestBlend = D3D12_BLEND_ONE;
            }
            if (desc.BlendOpAlpha == D3D12_BLEND_OP_MIN ||
                desc.BlendOpAlpha == D3D12_BLEND_OP_MAX)
            {
                desc.SrcBlendAlpha = D3D12_BLEND_ONE;
                desc.DestBlendAlpha = D3D12_BLEND_ONE;
            }

            desc.LogicOp = D3D12_LOGIC_OP_NOOP;
            desc.RenderTargetWriteMask = Converters::GetColorWriteMask(attachment.ColorWriteMask);
            return desc;
        }

        D3D12_DEPTH_STENCILOP_DESC GetStencilFace(const SStencilOpState& state)
        {
            return {
                .StencilFailOp = Converters::GetStencilOp(state.FailOp),
                .StencilDepthFailOp = Converters::GetStencilOp(state.DepthFailOp),
                .StencilPassOp = Converters::GetStencilOp(state.PassOp),
                .StencilFunc = Converters::GetComparisonFunc(state.CompareOp),
            };
        }
    }

    D3D12GraphicsPipeline::D3D12GraphicsPipeline(
        const GraphicsContext* context,
        const SPipelineCreateInfo& info
    ) : GraphicsPipeline(context, info)
    {
        m_D3D12Context = static_cast<const D3D12GraphicsContext*>(context);
        const auto shader = std::static_pointer_cast<D3D12Shader>(m_Shader);
        m_RootSignature = shader->GetRootSignature();
        m_PrimitiveTopology = Converters::GetPrimitiveTopology(m_InputAssembly.Topology);
        CreateInputLayout();
        CreatePipeline();
    }

    void D3D12GraphicsPipeline::Bind(const Ref<CommandBuffer>& cmd)
    {
        cmd->BindPipeline(this);
        m_Shader->Bind(cmd, this);
    }

    void D3D12GraphicsPipeline::CreatePipeline()
    {
        auto vertexModule = std::static_pointer_cast<D3D12ShaderModule>(
            m_Shader->GetModule(EShaderStage::Vertex)
        );
        auto pixelModule = std::static_pointer_cast<D3D12ShaderModule>(
            m_Shader->GetModule(EShaderStage::Pixel)
        );

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
        desc.pRootSignature = m_RootSignature.Get();
        desc.VS = vertexModule ? vertexModule->GetBytecode() : D3D12_SHADER_BYTECODE{};
        desc.PS = pixelModule ? pixelModule->GetBytecode() : D3D12_SHADER_BYTECODE{};
        desc.BlendState.AlphaToCoverageEnable = m_Multisample.AlphaToCoverageEnable;
        desc.BlendState.IndependentBlendEnable = FALSE;
        desc.BlendState.RenderTarget[0] = m_ColorBlend.Attachments.empty()
            ? D3D12_RENDER_TARGET_BLEND_DESC{
                .BlendEnable = FALSE,
                .LogicOpEnable = FALSE,
                .SrcBlend = D3D12_BLEND_ONE,
                .DestBlend = D3D12_BLEND_ZERO,
                .BlendOp = D3D12_BLEND_OP_ADD,
                .SrcBlendAlpha = D3D12_BLEND_ONE,
                .DestBlendAlpha = D3D12_BLEND_ZERO,
                .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                .LogicOp = D3D12_LOGIC_OP_NOOP,
                .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
            }
            : GetBlendAttachment(m_ColorBlend.Attachments[0]);
        desc.SampleMask = UINT_MAX;
        desc.RasterizerState.FillMode = Converters::GetFillMode(m_Rasterization.PolygonMode);
        desc.RasterizerState.CullMode = Converters::GetCullMode(m_Rasterization.CullMode);
        desc.RasterizerState.FrontCounterClockwise =
            Converters::GetFrontCounterClockwise(m_Rasterization.FrontFace);
        desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        desc.RasterizerState.DepthClipEnable = TRUE;
        desc.RasterizerState.MultisampleEnable = FALSE;
        desc.RasterizerState.AntialiasedLineEnable = FALSE;
        desc.RasterizerState.ForcedSampleCount = 0;
        desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        desc.DepthStencilState.DepthEnable = m_DepthStencil.DepthTestEnable;
        desc.DepthStencilState.DepthWriteMask = m_DepthStencil.DepthWriteEnable
            ? D3D12_DEPTH_WRITE_MASK_ALL
            : D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthStencilState.DepthFunc = Converters::GetComparisonFunc(m_DepthStencil.DepthCompareOp);
        desc.DepthStencilState.StencilEnable = m_DepthStencil.StencilTestEnable;
        desc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        desc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        desc.DepthStencilState.FrontFace = GetStencilFace(m_DepthStencil.Front);
        desc.DepthStencilState.BackFace = GetStencilFace(m_DepthStencil.Back);

        desc.InputLayout = {
            .pInputElementDescs = m_InputElements.data(),
            .NumElements = (UINT)m_InputElements.size(),
        };
        desc.PrimitiveTopologyType = Converters::GetPrimitiveTopologyType(m_InputAssembly.Topology);
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = Converters::GetFormat(m_ColorAttachmentFormat);
        desc.DSVFormat = m_DepthAttachmentFormat == EImageFormat::Undefined
            ? DXGI_FORMAT_UNKNOWN
            : Converters::GetFormat(m_DepthAttachmentFormat);
        desc.SampleDesc.Count = Converters::GetSampleCount(m_Multisample.RasterizationSamples);
        desc.SampleDesc.Quality = 0;
        desc.NodeMask = 0;
        desc.CachedPSO = {};
        desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        const auto result = m_D3D12Context->GetDevice()->CreateGraphicsPipelineState(
            &desc,
            IID_PPV_ARGS(&m_PipelineState)
        );
        DX_CHECK_RESULT(result);
    }

    void D3D12GraphicsPipeline::CreateInputLayout()
    {
        m_InputElements.clear();
        m_InputSemanticNames.clear();
        m_InputElements.reserve(m_BufferLayout.GetAttributeCount());
        m_InputSemanticNames.reserve(m_BufferLayout.GetAttributeCount());

        for (const auto& binding : m_BufferLayout)
        {
            for (const auto& attribute : binding)
            {
                auto semantic = Converters::GetInputSemantic(binding, attribute);
                m_InputSemanticNames.push_back(std::move(semantic.Name));

                m_InputElements.push_back({
                    .SemanticName = m_InputSemanticNames.back().c_str(),
                    .SemanticIndex = semantic.Index,
                    .Format = Converters::GetFormat(attribute.GetType()),
                    .InputSlot = binding.GetBinding(),
                    .AlignedByteOffset = (UINT)attribute.GetOffset(),
                    .InputSlotClass = binding.GetInputRate() == EInputRate::Instance
                        ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                        : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                    .InstanceDataStepRate = binding.GetInputRate() == EInputRate::Instance ? 1u : 0u,
                });
            }
        }
    }

    D3D12ComputePipeline::D3D12ComputePipeline(
        const GraphicsContext* context,
        const SPipelineCreateInfo& info
    ) : ComputePipeline(context, info)
    {
        m_D3D12Context = static_cast<const D3D12GraphicsContext*>(context);
        const auto shader = std::static_pointer_cast<D3D12Shader>(m_Shader);
        m_RootSignature = shader->GetRootSignature();
        CreatePipeline();
    }

    void D3D12ComputePipeline::Bind(const Ref<CommandBuffer>& cmd)
    {
        cmd->BindPipeline(this);
        m_Shader->Bind(cmd, this);
    }

    void D3D12ComputePipeline::CreatePipeline()
    {
        const auto computeModule = std::static_pointer_cast<D3D12ShaderModule>(
            m_Shader->GetModule(EShaderStage::Compute)
        );
        EE_CORE_ASSERT(computeModule, "Compute shader module not found in shader!")

        D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
        desc.pRootSignature = m_RootSignature.Get();
        desc.CS = computeModule->GetBytecode();

        DX_CHECK_RESULT(
            m_D3D12Context->GetDevice()->CreateComputePipelineState(
                &desc,
                IID_PPV_ARGS(&m_PipelineState)
            )
        );
    }
}

#endif
