#include "epch.h"
#include "GraphicsContext.h"

#include <Engine/Graphics/CommandBuffer.h>

#include <Graphics/Vulkan/VulkanGraphicsContext.h>

namespace Elixir
{
    Scope<GraphicsContext> GraphicsContext::Create(const EGraphicsAPI api, const Window* window)
    {
        switch (api)
        {
            case EGraphicsAPI::Vulkan:
                return CreateScope<Vulkan::VulkanGraphicsContext>(api, window);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    Ref<CommandBuffer> GraphicsContext::CreateCommandBuffer()
    {
        return CommandBuffer::Create(this);
    }
}