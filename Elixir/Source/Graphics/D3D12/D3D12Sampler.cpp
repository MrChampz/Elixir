#include "epch.h"
#include "D3D12Sampler.h"

#ifdef EE_PLATFORM_WINDOWS

namespace Elixir::D3D12
{
    D3D12Sampler::D3D12Sampler(
        const GraphicsContext* context,
        const SSamplerCreateInfo& info
    ) : Sampler(context, info)
    {
        m_D3D12Context = static_cast<const D3D12GraphicsContext*>(context);
        m_Descriptor = m_D3D12Context->GetSamplerDescriptorHeap().Allocate();
        CreateSampler(m_Descriptor.CPU);
    }

    D3D12Sampler::~D3D12Sampler() = default;

    void D3D12Sampler::CreateSampler(const D3D12_CPU_DESCRIPTOR_HANDLE destination) const
    {
        D3D12_SAMPLER_DESC desc = {};
        desc.Filter = Converters::GetFilter(this);
        desc.AddressU = Converters::GetAddressMode(m_AddressModeU);
        desc.AddressV = Converters::GetAddressMode(m_AddressModeV);
        desc.AddressW = Converters::GetAddressMode(m_AddressModeW);
        desc.MipLODBias = m_MipLodBias;
        desc.MaxAnisotropy = m_AnisotropyEnable ? (UINT)std::max(1.0f, m_MaxAnisotropy) : 1;
        desc.ComparisonFunc = m_CompareEnable
            ? Converters::GetSamplerComparisonFunc(m_CompareOp)
            : D3D12_COMPARISON_FUNC_NEVER;
        desc.MinLOD = m_MinLod;
        desc.MaxLOD = m_MaxLod == 0.0f ? D3D12_FLOAT32_MAX : m_MaxLod;
        Converters::GetBorderColor(m_BorderColor, desc.BorderColor);

        m_D3D12Context->GetDevice()->CreateSampler(&desc, destination);
    }
}

#endif
