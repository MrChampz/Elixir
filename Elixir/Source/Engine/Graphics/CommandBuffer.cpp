#include "epch.h"
#include "CommandBuffer.h"

namespace Elixir
{
    void CommandBuffer::SetPushConstant(
        const Ref<PushConstantBuffer>& buffer,
        const Ref<Shader>& shader,
        const EShaderStage stages
    )
    {
        SetPushConstant(buffer, shader.get(), stages);
    }

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
        const std::span<SBufferCopy> regions
    )
    {
        src->Copy(this, dst, regions);
    }
}