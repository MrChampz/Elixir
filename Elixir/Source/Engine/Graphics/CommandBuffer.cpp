#include "epch.h"
#include "CommandBuffer.h"

namespace Elixir
{
    Ref<CommandBuffer> CommandBuffer::Create(GraphicsContext* context)
    {
        switch (GraphicsContext::GetGraphicsAPI())
        {
            case EGraphicsAPI::Vulkan:
                return nullptr;
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }
}