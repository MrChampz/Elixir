#pragma once

#include <Engine/Graphics/CommandBuffer.h>

namespace Elixir::GUI
{
    class RenderBatch;

    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;

        virtual void GenerateDrawCommands(const RenderBatch& batch) = 0;
        virtual void Render(const Ref<CommandBuffer>& cmd) = 0;
        virtual bool HasData() const = 0;
        virtual void Clear() = 0;
    };
}