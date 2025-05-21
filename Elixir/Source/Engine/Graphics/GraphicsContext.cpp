#include "epch.h"
#include "GraphicsContext.h"

#include <Engine/Graphics/CommandBuffer.h>

#include <Graphics/Vulkan/VulkanGraphicsContext.h>

namespace Elixir
{
    EGraphicsAPI GraphicsContext::s_API = EGraphicsAPI::Unknown;

    Scope<GraphicsContext> GraphicsContext::Create(const EGraphicsAPI api, const Window* window)
    {
        s_API = api;

        switch (s_API)
        {
            case EGraphicsAPI::Vulkan:
                return CreateScope<Vulkan::VulkanGraphicsContext>(window);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    Ref<CommandBuffer> GraphicsContext::CreateCommandBuffer()
    {
        return nullptr;
    }
}