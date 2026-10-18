#include "epch.h"
#include "SkinningBatch.h"

namespace Elixir
{
    void SkinningBatch::Reset()
    {
        m_Jobs.clear();
        m_Blocks.clear();
        m_Matrices.clear();
    }

    bool SkinningBatch::Append(
        const SkeletalMesh& mesh,
        const std::span<const glm::mat4> modelPose,
        const uint32_t sourceVertexOffset,
        const uint32_t outputVertexOffset,
        const uint32_t boneCapacity
    )
    {
        const auto& data = mesh.GetData();
        const auto count = data.BoneMap.size();

        if (modelPose.size() != mesh.GetSkeleton()->GetBones().size() ||
            m_Matrices.size() > boneCapacity ||
            count > (boneCapacity - m_Matrices.size()) ||
            data.Vertices.size() > (UINT32_MAX - sourceVertexOffset) ||
            data.Vertices.size() > (UINT32_MAX - outputVertexOffset))
        {
            EE_CORE_ERROR("GPU skinning requires a complete pose and sufficient palette and vertex capacity.")
            return false;
        }

        const auto firstBone = (uint32_t)m_Matrices.size();
        for (size_t i = 0; i < count; ++i)
        {
            const auto matrix = modelPose[data.BoneMap[i]] * data.InverseBindMatrices[i];
            for (int c = 0; c < 4; ++c)
            {
                if ((glm::any(glm::isnan(matrix[c])) || glm::any(glm::isinf(matrix[c]))))
                {
                    m_Matrices.resize(firstBone);
                    EE_CORE_ERROR("GPU skinning received a nonfinite pose matrix.")
                    return false;
                }
            }

            m_Matrices.push_back(matrix);
        }

        const auto jobIndex = (uint32_t)m_Jobs.size();
        const auto vertexCount = (uint32_t)data.Vertices.size();

        m_Jobs.push_back({
            sourceVertexOffset,
            outputVertexOffset,
            firstBone,
            vertexCount
        });

        for (uint32_t first = 0; first < vertexCount;)
        {
            m_Blocks.push_back({ jobIndex, first });

            if (vertexCount - first <= GROUP_SIZE) break;
            first += GROUP_SIZE;
        }

        return true;
    }
}
