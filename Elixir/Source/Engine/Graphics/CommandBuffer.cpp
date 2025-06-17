#include "epch.h"
#include "CommandBuffer.h"

#include <Graphics/Vulkan/VulkanCommandBuffer.h>

namespace Elixir
{
    void CommandBuffer::CopyBuffer(
        const Ref<Buffer>& src,
        const Ref<Buffer>& dst,
        const std::span<SBufferCopy> regions
    )
    {
        src->Copy(this, dst, regions);
    }

    void CommandBuffer::CopyBuffer(
        const Ref<Buffer>& src,
        const Buffer* dst,
        std::span<SBufferCopy> regions
    )
    {
        src->Copy(this, dst, regions);
    }

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