#include "epch.h"
#include "GraphicsContext.h"

#include <Graphics/Vulkan/VulkanGraphicsContext.h>

namespace Elixir
{
    void GraphicsContext::Clear()
    {
        ClearImage(m_RenderTarget);
    }

    void GraphicsContext::Clear(const Ref<Image>& image)
    {
        ClearImage(image);
    }

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
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }
}