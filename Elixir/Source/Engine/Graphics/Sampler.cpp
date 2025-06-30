#include "epch.h"
#include "Sampler.h"

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

		m_AnisotropyEnabled = info.AnisotropyEnabled;
		m_MaxAnisotropy = info.MaxAnisotropy;

        m_CompareEnabled = info.CompareEnabled;
        m_CompareOp = info.CompareOp;

		m_BorderColor = info.BorderColor;

        m_UnnormalizedCoordinates = info.UnnormalizedCoordinates;
    }
}