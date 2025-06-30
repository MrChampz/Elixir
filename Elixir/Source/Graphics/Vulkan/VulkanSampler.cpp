#include "epch.h"
#include "VulkanSampler.h"

#include <Graphics/Vulkan/Utils.h>

namespace Elixir::Vulkan
{
    VulkanSampler::VulkanSampler(
        const GraphicsContext* context,
        const SSamplerCreateInfo& info
    ) : Sampler(context, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);
        Create(info);
    }

    VulkanSampler::~VulkanSampler()
    {
        EE_PROFILE_ZONE_SCOPED()
        Destroy();
    }

    void VulkanSampler::Create(const SSamplerCreateInfo& info)
    {
        EE_PROFILE_ZONE_SCOPED()

        const auto samplerInfo = Initializers::SamplerCreateInfo(info);

        VK_CHECK_RESULT(
            vkCreateSampler(
                m_GraphicsContext->GetDevice(),
                &samplerInfo,
                nullptr,
                &m_Sampler
            )
        );
    }

    void VulkanSampler::Destroy()
    {
        EE_PROFILE_ZONE_SCOPED()

        if (!IsValid()) return;

        vkDestroySampler(m_GraphicsContext->GetDevice(), m_Sampler, nullptr);
    }
}