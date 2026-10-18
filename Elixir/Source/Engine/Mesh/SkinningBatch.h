#pragma once

#include <Engine/Mesh/SkeletalMesh.h>

namespace Elixir
{
    /** @brief Maps a mesh's source and output ranges to its frame-local bone palette. */
    struct SSkinningJob
    {
        uint32_t SourceVertexOffset = 0;
        uint32_t OutputVertexOffset = 0;
        uint32_t BoneMatrixOffset = 0;
        uint32_t VertexCount = 0;
    };

    /** @brief Assigns one compute workgroup to a block of vertices in a skinning job. */
    struct SSkinningBlock
    {
        uint32_t JobIndex = 0;
        uint32_t FirstVertex = 0;
    };

    /** @brief Collects palettes and work blocks for one batched GPU skinning dispatch. */
    class ELIXIR_API SkinningBatch final
    {
    public:
        /** Number of vertices processed by a compute workgroup. */
        static constexpr uint32_t GROUP_SIZE = 128;

        /** @brief Reuse storage for a new frame. */
        void Reset();

        /**
         * @brief Append a pose and its work blocks; leave the batch unchanged on failure.
         * @param sourceVertexOffset Vertex offset in a pool of SSkeletalMeshVertex.
         * @param outputVertexOffset Vertex offset in a pool of SStaticMeshVertex.
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

        /** @brief Get one entry per compute workgroup. */
        const std::vector<SSkinningBlock>& GetBlocks() const { return m_Blocks; }

        /** @brief Get column-major skin matrices, grouped by job. */
        const std::vector<glm::mat4>& GetMatrices() const { return m_Matrices; }

    private:
        std::vector<SSkinningJob> m_Jobs;
        std::vector<SSkinningBlock> m_Blocks;
        std::vector<glm::mat4> m_Matrices;
    };
}
