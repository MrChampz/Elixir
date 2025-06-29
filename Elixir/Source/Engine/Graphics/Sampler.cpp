#include "epch.h"
#include "Sampler.h"

#include <Graphics/Vulkan/VulkanSampler.h>

namespace Elixir
{
    Ref<Sampler> Sampler::Create(const GraphicsContext* context)
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanSampler>(context);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }
    
    Sampler::Sampler(const GraphicsContext* context)
        : m_GraphicsContext(context)
    {
        m_MagFilter = ESamplerFilter::Linear;
		m_MinFilter = ESamplerFilter::Linear;

		m_MipmapMode = ESamplerMipmapMode::Linear;
		
		m_AddressModeU = ESamplerAddressMode::Repeat;
		m_AddressModeV = ESamplerAddressMode::Repeat;
		m_AddressModeW = ESamplerAddressMode::Repeat;
		
		m_MipLodBias = 0;
		m_MinLod = 0;
		m_MaxLod = 0;

		m_AnisotropyEnabled = false;
		m_MaxAnisotropy = 0;

		m_BorderColor = ESamplerBorderColor::FloatTransparentBlack;
    }
}