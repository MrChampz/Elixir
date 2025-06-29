#include "epch.h"
#include "VulkanSampler.h"

#include "Utils.h"

namespace Elixir
{
    Vulkan::VulkanSampler::VulkanSampler(const GraphicsContext* context)
        : Sampler(context)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);
        UpdateSampler();
    }

    Vulkan::VulkanSampler::~VulkanSampler()
    {
        EE_PROFILE_ZONE_SCOPED()
        Destroy();
    }

    void Vulkan::VulkanSampler::Destroy()
    {
        EE_PROFILE_ZONE_SCOPED()

        if (m_Destroyed) return;

        vkDestroySampler(m_GraphicsContext->GetDevice(), m_Sampler, nullptr);
        m_Destroyed = true;
    }

    void Vulkan::VulkanSampler::UpdateSampler()
    {
        EE_PROFILE_ZONE_SCOPED()
        
        Destroy();

		VkSamplerCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.magFilter = Converters::GetSamplerFilter(m_MagFilter);
		info.minFilter = Converters::GetSamplerFilter(m_MinFilter);
		info.mipmapMode = Converters::GetSamplerMipmapMode(m_MipmapMode);
		info.addressModeU = Converters::GetSamplerAddressMode(m_AddressModeU);
		info.addressModeV = Converters::GetSamplerAddressMode(m_AddressModeV);
		info.addressModeW = Converters::GetSamplerAddressMode(m_AddressModeW);
		info.mipLodBias = m_MipLodBias;
		info.minLod = m_MinLod;
		info.maxLod = m_MaxLod;
		info.anisotropyEnable = m_AnisotropyEnabled;
		info.maxAnisotropy = m_MaxAnisotropy;
		info.borderColor = Converters::GetSamplerBorderColor(m_BorderColor);

		VK_CHECK_RESULT(
		    vkCreateSampler(
		        m_GraphicsContext->GetDevice(),
		        &info,
		        nullptr,
		        &m_Sampler
		    )
		);

		m_Destroyed = false;
    }
}