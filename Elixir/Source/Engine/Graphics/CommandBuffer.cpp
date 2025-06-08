#include "epch.h"
#include "CommandBuffer.h"

#include <Graphics/Vulkan/VulkanCommandBuffer.h>

namespace Elixir
{
    Ref<CommandBuffer> CommandBuffer::Create(GraphicsContext* context)
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanCommandBuffer>(context);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }
}