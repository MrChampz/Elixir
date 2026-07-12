#include "epch.h"
#include "GraphicsContext.h"

#ifdef EE_PLATFORM_WINDOWS
#include <Graphics/D3D12/D3D12GraphicsContext.h>
#endif
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

namespace Elixir
{
    float GraphicsContext::GetDPIScale() const
    {
        return m_Window->GetDPIScale();
    }

    Scope<GraphicsContext> GraphicsContext::Create(const EGraphicsAPI api, Executor* executor, const Window* window)
    {
        switch (api)
        {
            case EGraphicsAPI::Vulkan:
                return CreateScope<Vulkan::VulkanGraphicsContext>(api, executor, window);
#ifdef EE_PLATFORM_WINDOWS
            case EGraphicsAPI::D3D12:
                return CreateScope<D3D12::D3D12GraphicsContext>(api, executor, window);
#endif
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }
}
