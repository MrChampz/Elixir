#include "epch.h"
#include "Sampler.h"

#ifdef EE_PLATFORM_WINDOWS
#include <Graphics/D3D12/D3D12Sampler.h>
#endif
#include <Graphics/Vulkan/VulkanSampler.h>

namespace Elixir
{
    Ref<Sampler> Sampler::Create(
        const GraphicsContext* context,
        const SSamplerCreateInfo& info
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanSampler>(context, info);
#ifdef EE_PLATFORM_WINDOWS
            case EGraphicsAPI::D3D12:
                return CreateRef<D3D12::D3D12Sampler>(context, info);
#endif
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }
    
    Sampler::Sampler(const GraphicsContext* context, const SSamplerCreateInfo& info)
        : m_GraphicsContext(context)
    {
        m_MagFilter = info.MagFilter;
		m_MinFilter = info.MinFilter;

		m_MipmapMode = info.MipmapMode;
		
		m_AddressModeU = info.AddressModeU;
		m_AddressModeV = info.AddressModeV;
		m_AddressModeW = info.AddressModeW;

		m_MipLodBias = info.MipLodBias;
		m_MinLod = info.MinLod;
		m_MaxLod = info.MaxLod;

		m_AnisotropyEnable = info.AnisotropyEnable;
		m_MaxAnisotropy = info.MaxAnisotropy;

        m_CompareEnable = info.CompareEnable;
        m_CompareOp = info.CompareOp;

		m_BorderColor = info.BorderColor;

        m_UnnormalizedCoordinates = info.UnnormalizedCoordinates;
    }
}
