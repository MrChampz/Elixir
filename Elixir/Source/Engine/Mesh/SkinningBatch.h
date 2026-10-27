#pragma once

#include <Engine/Mesh/SkeletalMesh.h>

namespace Elixir
{
    /** Maps one submitted instance's vertex ranges to its skin matrices in the batch. */
    struct SSkinningJob
    {
        /** First source vertex, in SSkeletalMeshVertex elements rather than bytes. */
        uint32_t SourceVertexOffset = 0;

        /** First output vertex, in SStaticMeshVertex elements rather than bytes. */
        uint32_t OutputVertexOffset = 0;

        /** First skin matrix in the batch palette, in glm::mat4 elements. */
        uint32_t BoneMatrixOffset = 0;

        /** Number of source vertices to deform into the output range. */
        uint32_t VertexCount = 0;
    };

    /** Assigns one compute workgroup to a contiguous block of a job's vertices. */
    struct SSkinningBlock
    {
        /** Index of the owning job in SkinningBatch::GetJobs(). */
        uint32_t JobIndex = 0;

        /** First vertex relative to the job, before adding its source or output offset. */
        uint32_t FirstVertex = 0;
    };

    /**
     * @brief Collect instance poses and work blocks on the CPU for GPU skinning.
     *
     * Each Append adds a job, its skin matrices, and one block per compute workgroup.
     * SkinningPass uploads these arrays and splits dispatches by the device's group limit.
     * This class does not allocate GPU geometry or deform vertices.
     */
    class ELIXIR_API SkinningBatch final
    {
    public:
        /** Vertices per workgroup; must match the compute shader's thread count along X. */
        static constexpr uint32_t GROUP_SIZE = 128;

        /** @brief Clear jobs, blocks, and matrices while retaining capacity for reuse. */
        void Reset();

        /**
         * @brief Append one instance's skin matrices, job, and work blocks.
         *
         * Builds each skin matrix from the model pose selected by BoneMap and the mesh's
         * inverse bind matrix. Source geometry can be shared by jobs with different poses
         * and output ranges. Validation failure leaves the batch unchanged.
         *
         * @param mesh Supplies source vertex count, bone mapping, and inverse bind matrices.
         * @param modelPose One skeleton-space matrix per bone in the mesh's Skeleton.
         * @param sourceVertexOffset First source vertex, in SSkeletalMeshVertex elements.
         * @param outputVertexOffset First output vertex, in SStaticMeshVertex elements.
         * @param boneCapacity Maximum total skin matrices allowed in the batch.
         * @return True if the job was appended. Returns false for an incomplete pose,
         * nonfinite skin matrices, insufficient bone capacity, or overflowing vertex ranges.
         */
        bool Append(
            const SkeletalMesh& mesh,
            std::span<const glm::mat4> modelPose,
            uint32_t sourceVertexOffset,
            uint32_t outputVertexOffset,
            uint32_t boneCapacity
        );

        /** @brief Get the jobs consumed by the compute shader. */
        const std::vector<SSkinningJob>& GetJobs() const { return m_Jobs; }

        /** @brief Get workgroup blocks containing a job index and its first vertex. */
        const std::vector<SSkinningBlock>& GetBlocks() const { return m_Blocks; }

        /** @brief Get column-major skin matrices addressed by each job's BoneMatrixOffset. */
        const std::vector<glm::mat4>& GetMatrices() const { return m_Matrices; }

    private:
        std::vector<SSkinningJob> m_Jobs;
        std::vector<SSkinningBlock> m_Blocks;
        std::vector<glm::mat4> m_Matrices;
    };
}
