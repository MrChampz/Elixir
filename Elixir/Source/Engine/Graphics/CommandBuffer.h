#pragma once

#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Graphics/GraphicsTypes.h>

namespace Elixir
{
    class Texture;
    class GraphicsPipeline;
    class ComputePipeline;
    class Buffer;
    class VertexBuffer;
    class DynamicVertexBuffer;
    class IndexBuffer;
    class DynamicIndexBuffer;

    enum class ECommandBufferLevel : uint8_t
    {
        Primary = 0,
        Secondary
    };

    class ELIXIR_API CommandBuffer
    {
    public:
        virtual ~CommandBuffer() = default;

        virtual void Begin(const SRenderingInfo& info = {}) = 0;
        virtual void End() = 0;

        virtual void Reset() = 0;

        /** Dispatching compute methods */

        virtual void Dispatch(
            uint32_t groupCountX,
            uint32_t groupCountY = 1,
            uint32_t groupCountZ = 1
        ) = 0;

        /** Drawing methods **/

        virtual void BeginRendering(const SRenderingInfo& info) = 0;
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

        /**
         * Binds a pipeline for Command Buffer execution.
         * NOTE: This method should not be used directly!
         * It's intended to be used by Pipeline implementation.
         *
         * @param pipeline ComputePipeline instance to be bound.
         */
        virtual void BindPipeline(const ComputePipeline* pipeline) = 0;

        virtual void BindVertexBuffers(
            std::span<const VertexBuffer*> vertexBuffers,
            std::span<uint64_t> offsets = {},
            uint32_t bindingCount = 1,
            uint32_t firstBinding = 0
        ) = 0;
        virtual void BindVertexBuffers(
            std::span<const DynamicVertexBuffer*> vertexBuffers,
            std::span<uint64_t> offsets = {},
            uint32_t bindingCount = 1,
            uint32_t firstBinding = 0
        ) = 0;
        virtual void BindVertexBuffers(
            std::span<const Buffer*> vertexBuffers,
            std::span<uint64_t> offsets = {},
            uint32_t bindingCount = 1,
            uint32_t firstBinding = 0
        ) = 0;
        virtual void BindIndexBuffer(const IndexBuffer* indexBuffer) = 0;
        virtual void BindIndexBuffer(const DynamicIndexBuffer* indexBuffer) = 0;
        virtual void BindIndexBuffer(
            const Buffer* indexBuffer,
            EIndexType indexType = EIndexType::UInt32
        ) = 0;

        template <typename T, typename... Args>
        void BindBuffer(const Buffer* buffer, Args... args);

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

        virtual void ExecuteCommands(std::span<Ref<CommandBuffer>> cmds) = 0;

        // virtual void Submit(
        //     const Ref<Pipeline>& pipeline,
        //     const Ref<VertexBuffer>& vertexBuffer,
        //     const Ref<IndexBuffer>& indexBuffer,
        //     uint32_t indexCount = 0
        // ) = 0;

        virtual void Flush() = 0;

        [[nodiscard]] ECommandBufferLevel GetLevel() const { return m_Level; }
        [[nodiscard]] bool IsPrimary() const { return m_Level == ECommandBufferLevel::Primary; }
        [[nodiscard]] bool IsSecondary() const { return m_Level == ECommandBufferLevel::Secondary; }

      protected:
        explicit CommandBuffer(const GraphicsContext* context, const ECommandBufferLevel level)
            : m_Level(level), m_GraphicsContext(context) {}

        ECommandBufferLevel m_Level;

        const GraphicsContext* m_GraphicsContext;
    };

    template <typename T, typename... Args>
    void CommandBuffer::BindBuffer(const Buffer* buffer, Args... args)
    {
        if constexpr (std::is_same_v<T, VertexBuffer> || std::is_same_v<T, DynamicVertexBuffer>)
        {
            const Buffer* buffers[] = { buffer };
            BindVertexBuffers(buffers, args...);
        }
        else if constexpr (std::is_same_v<T, IndexBuffer> || std::is_same_v<T, DynamicIndexBuffer>)
        {
            BindIndexBuffer(buffer, args...);
        }
        else
        {
            EE_CORE_ASSERT(false, "Unsupported buffer type!")
        }
    }
}