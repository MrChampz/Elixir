#pragma once

#include <Engine/Graphics/FrameSlotState.h>
#include <Engine/Graphics/Buffer.h>
#include <Engine/Mesh/SkinningBatch.h>

namespace Elixir
{
    class ShaderLoader;
    class ComputePipeline;
    class Shader;

    /** @brief Records batched compute skinning into a shared vertex buffer. */
    class ELIXIR_API SkinningPass final
    {
    public:
        /** @brief Create a compute pipeline; context must outlive the pass. */
        SkinningPass(const GraphicsContext& context, const ShaderLoader& shaderLoader);

        /** @brief Check whether the compute shader and pipeline are available. */
        bool IsValid() const;

        /**
         * @brief Upload palettes and jobs, dispatch skinning, and publish vertex reads.
         * @param dstStage First GPU stage that consumes the deformed vertices.
         * @param dstAccess Access used by that consumer.
         * @pre cmd is recording outside a render pass on a graphics queue supporting compute.
         * @pre The current frame slot fence completed; source and output ranges fit their buffers.
         * @pre Call at most once per frame slot before submitting its commands to the GPU.
         */
        bool Record(
            const Ref<CommandBuffer>& cmd,
            const SkinningBatch& batch,
            const Ref<StorageBuffer>& source,
            const Ref<StorageBuffer>& output,
            EPipelineStage dstStage = EPipelineStage::VertexInput,
            EPipelineAccess dstAccess = EPipelineAccess::VertexAttributeRead
        );

    private:
        struct SFrameBuffers
        {
            Ref<DynamicStorageBuffer> Matrices;
            Ref<DynamicStorageBuffer> Jobs;
            Ref<DynamicStorageBuffer> Blocks;
        };

        // Grow a frame-slot upload buffer while preserving amortized allocation cost.
        bool EnsureBuffer(Ref<DynamicStorageBuffer>& buffer, size_t bytes);

        Ref<Shader> m_Shader;
        Ref<ComputePipeline> m_Pipeline;
        FrameSlotState<SFrameBuffers> m_FrameBuffers;

        const GraphicsContext& m_Context;
    };
}
