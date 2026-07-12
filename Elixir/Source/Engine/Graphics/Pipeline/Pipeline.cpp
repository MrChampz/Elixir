#include "epch.h"
#include "Pipeline.h"

#ifdef EE_PLATFORM_WINDOWS
#include <Graphics/D3D12/D3D12Pipeline.h>
#endif
#include <Graphics/Vulkan/VulkanPipeline.h>
#include <Engine/Graphics/Converters.h>

namespace Elixir
{
    /* GraphicsPipeline */

    Ref<GraphicsPipeline> GraphicsPipeline::Create(
        const GraphicsContext* context,
        const SPipelineCreateInfo& info
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanGraphicsPipeline>(context, std::move(info));
#ifdef EE_PLATFORM_WINDOWS
            case EGraphicsAPI::D3D12:
                return CreateRef<D3D12::D3D12GraphicsPipeline>(context, std::move(info));
#endif
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    GraphicsPipeline::GraphicsPipeline(
        const GraphicsContext* context,
        const SPipelineCreateInfo& info
    ) : Pipeline(info), m_GraphicsContext(context)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_InputAssembly = info.InputAssembly;
        m_Viewport = info.Viewport;
        m_Rasterization = info.Rasterization;
        m_Multisample = info.Multisample;
        m_ColorBlend = info.ColorBlend;
        m_DepthStencil = info.DepthStencil;
        m_ColorAttachmentFormat = info.ColorAttachmentFormat;
        m_DepthAttachmentFormat = Graphics::Converters::GetImageFormat(info.DepthAttachmentFormat);
        m_BufferLayout = info.VertexBufferLayout;
    }

    /* ComputePipeline */

    Ref<ComputePipeline> ComputePipeline::Create(
        const GraphicsContext* context,
        const SPipelineCreateInfo& info
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanComputePipeline>(context, std::move(info));
#ifdef EE_PLATFORM_WINDOWS
            case EGraphicsAPI::D3D12:
                return CreateRef<D3D12::D3D12ComputePipeline>(context, std::move(info));
#endif
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    ComputePipeline::ComputePipeline(
        const GraphicsContext* context,
        const SPipelineCreateInfo& info
    ) : Pipeline(info), m_GraphicsContext(context)
    {
        EE_PROFILE_ZONE_SCOPED()
    }
}
