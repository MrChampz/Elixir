#pragma once

#ifdef EE_PLATFORM_WINDOWS

#include <Engine/Graphics/Sampler.h>
#include <Graphics/D3D12/D3D12GraphicsContext.h>

namespace Elixir::D3D12
{
    class ELIXIR_API D3D12Sampler final : public Sampler
    {
      public:
        D3D12Sampler(const GraphicsContext* context, const SSamplerCreateInfo& info);
        ~D3D12Sampler() override;

        bool IsValid() const override { return m_Descriptor.IsValid(); }
        UINT GetDescriptorIndex() const { return m_Descriptor.Index; }

        void CreateSampler(D3D12_CPU_DESCRIPTOR_HANDLE destination) const;

      private:
        void Destroy() override {}

        SD3D12DescriptorAllocation m_Descriptor;
        const D3D12GraphicsContext* m_D3D12Context = nullptr;
    };
}

#endif
