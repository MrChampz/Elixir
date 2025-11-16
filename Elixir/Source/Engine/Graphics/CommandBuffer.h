#pragma once

#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Graphics/Buffer.h>

namespace Elixir
{
    class Texture;
    class GraphicsPipeline;

    class ELIXIR_API CommandBuffer
    {
    public:
        virtual ~CommandBuffer() = default;

        virtual void Begin() = 0;
        virtual void End() = 0;

        virtual void Reset() = 0;

        /** Drawing methods **/

        //virtual void BeginRendering(const Ref<Texture>& colorAttachment, Extent2D renderArea) = 0;
        virtual void BeginRendering(
            const Ref<Texture>& colorAttachment,
            const Ref<DepthStencilImage>& depthStencilAttachment,
            Extent2D renderArea
        ) = 0;
        virtual void EndRendering() = 0;

        virtual void Draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t firstVertex = 0,
            uint32_t firstInstance = 0
        ) = 0;

        virtual void DrawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount = 1,
            uint32_t firstIndex = 0,
            uint32_t vertexOffset = 0,
            uint32_t firstInstance = 0
        ) = 0;

        /** Set methods **/

        virtual void SetViewports(
            const std::vector<Viewport>& viewports,
            uint32_t firstViewport = 0
        ) = 0;

        virtual void SetScissors(
            const std::vector<Rect2D>& scissors,
            uint32_t firstScissor = 0
        ) = 0;

        /** Bind methods **/

        /**
         * Binds a pipeline for Command Buffer execution.
         * NOTE: This method should not be used directly!
         * It's intended to be used by Pipeline implementation.
         *
         * @param pipeline GraphicsPipeline instance to be bound.
         */
        virtual void BindPipeline(const GraphicsPipeline* pipeline) = 0;

        // virtual void BindVertexBuffers(
        //     std::span<const Ref<VertexBuffer>> vertexBuffers,
        //     std::span<uint64_t> offsets = {},
        //     uint32_t bindingCount = 1,
        //     uint32_t firstBinding = 0
        // ) = 0;
        // virtual void BindIndexBuffer(const Ref<IndexBuffer> indexBuffer) = 0;

        /** Resource handling methods **/

        // virtual void TransitionImage(Image* image, EImageLayout layout);
        // virtual void TransitionImage(const Ref<Image>& image, EImageLayout layout);

        virtual void CopyBuffer(
            const Ref<Buffer>& src,
            const Ref<Buffer>& dst,
            std::span<SBufferCopy> regions = {}
        );

        virtual void CopyBuffer(
            const Ref<Buffer>& src,
            const Buffer* dst,
            std::span<SBufferCopy> regions = {}
        );

        // virtual void CopyBufferToImage(
        //     const Ref<BufferBase>& src,
        //     const Ref<Image>& dst,
        //     std::span<BufferImageCopy> regions = {}
        // ) = 0;

        // virtual void CopyBufferToImage(
        //     const Ref<BufferBase>& src,
        //     Image* dst,
        //     std::span<BufferImageCopy> regions = {}
        // ) = 0;

        // virtual void CopyImage(const Ref<Image>& src, const Ref<Image>& dst);
        // virtual void CopyImage(const Ref<Image>& src, const Ref<Image>& dst, Extent3D dstExtent);

        /** Submit and execution methods **/

        // virtual void Submit(
        //     const Ref<Pipeline>& pipeline,
        //     const Ref<VertexBuffer>& vertexBuffer,
        //     const Ref<IndexBuffer>& indexBuffer,
        //     uint32_t indexCount = 0
        // ) = 0;

        virtual void Flush() = 0;

        static Ref<CommandBuffer> Create(GraphicsContext* context);
    };
}