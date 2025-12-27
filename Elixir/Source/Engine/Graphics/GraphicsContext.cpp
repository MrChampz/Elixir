#include "epch.h"
#include "GraphicsContext.h"

#include <Graphics/Vulkan/VulkanGraphicsContext.h>

namespace Elixir
{
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