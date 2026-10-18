#pragma once

#include <Engine/Graphics/FrameSlotState.h>
#include <Engine/Graphics/Buffer.h>
#include <Engine/Mesh/SkinningBatch.h>

namespace Elixir
{
    class ShaderLoader;
    class ComputePipeline;
    class Shader;

    /**
     * @brief Record batched compute skinning into a shared GPU output buffer.
     *
     * Owns the compute shader, pipeline, and per-slot uploads for matrices, jobs, and blocks.
     * Reads persistent source vertices and writes output in SStaticMeshVertex layout.
     * The caller owns the geometry buffers and submits the recorded commands.
     */
    class ELIXIR_API SkinningPass final
    {
    public:
        /**
         * @brief Load the skinning compute shader and create its pipeline.
         * @param context Graphics context used to create resources and query device limits.
         * @param shaderLoader Loads the Skinning compute shader from the shader directory.
         * @pre The graphics context outlives this pass.
         */
        SkinningPass(const GraphicsContext& context, const ShaderLoader& shaderLoader);

        /** @brief Check whether the compute shader and pipeline are available. */
        bool IsValid() const;

        /**
         * @brief Upload the batch and record skinning dispatches and buffer barriers.
         *
         * Binds source vertices, skin matrices, jobs, blocks, and output to the shader.
         * Splits dispatches by the context's X workgroup limit. The final barrier makes output
         * writes available to the requested consumer; no vertices are copied back to the CPU.
         *
         * @param cmd Recording command buffer for bindings, barriers, and dispatches.
         * @param batch Captured instance palettes, vertex ranges, and workgroup assignments.
         * @param source Readable source buffer with the SSkeletalMeshVertex byte layout.
         * @param output Writable destination buffer with the SStaticMeshVertex byte layout.
         * @param dstStage First GPU stage that reads the output after the dispatches.
         * @param dstAccess Access performed by that consumer, such as vertex attribute reads.
         * @return True if recording succeeded or a valid call had no work blocks.
         * Returns false for invalid resources, limits, job ranges, or upload allocations.
         * @pre cmd records outside a render pass on a graphics queue that supports compute.
         * @pre Source and output buffers are distinct and outlive the submitted GPU work.
         * @pre The current frame slot's GPU work completed before its uploads are reused.
         * @pre Call at most once per slot use, before submitting that slot's commands.
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
        /** Retains independent upload buffers for one frame slot in flight. */
        struct SFrameBuffers
        {
            /** Column-major skin matrices, addressed by SSkinningJob::BoneMatrixOffset. */
            Ref<DynamicStorageBuffer> Matrices;

            /** Vertex ranges and palette offsets, one entry per submitted instance. */
            Ref<DynamicStorageBuffer> Jobs;

            /** Job index and first vertex for each compute workgroup. */
            Ref<DynamicStorageBuffer> Blocks;
        };

        /**
         * @brief Reuse or grow an upload buffer to hold the requested byte count.
         * @param buffer Upload buffer to keep or replace with a larger allocation.
         * @param bytes Required buffer size in bytes.
         * @return True if a valid buffer has sufficient capacity; false if growth fails or
         * the requested allocation size is unsupported.
         */
        bool EnsureBuffer(Ref<DynamicStorageBuffer>& buffer, size_t bytes);

        Ref<Shader> m_Shader;
        Ref<ComputePipeline> m_Pipeline;
        FrameSlotState<SFrameBuffers> m_FrameBuffers;

        const GraphicsContext& m_Context;
    };
}
