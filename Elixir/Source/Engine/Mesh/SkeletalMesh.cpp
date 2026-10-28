#include "epch.h"
#include "SkeletalMesh.h"

#include <Engine/Mesh/StaticMesh.h>

#include <glm/gtc/matrix_inverse.hpp>

namespace Elixir
{
    namespace
    {
        template<typename T>
        bool FiniteVector(const T& v)
        {
            for (glm::length_t i = 0; i < v.length(); ++i)
                if (!std::isfinite(v[i])) return false;
            return true;
        }

        bool ValidMatrix(const glm::mat4& matrix)
        {
            for (int i = 0; i < 4; ++i)
                if (!FiniteVector(matrix[i])) return false;

            return std::abs(matrix[0][3]) < 1e-5f &&
                std::abs(matrix[1][3]) < 1e-5f &&
                std::abs(matrix[2][3]) < 1e-5f &&
                std::abs(matrix[3][3] - 1.0f) < 1e-5f &&
                std::abs(glm::determinant(glm::mat3(matrix))) >= 1e-12f;
        }
    }

    Ref<SkeletalMesh> SkeletalMesh::Create(SSkeletalMeshData data)
    {
        const auto invalidMaterial =  std::ranges::any_of(
            data.Materials,
            [](const auto& material) { return !material; }
        );

        if (!data.Skeleton ||
            data.Vertices.empty() ||
            data.Indices.empty() ||
            data.Sections.empty() ||
            data.Vertices.size() > UINT32_MAX ||
            data.Indices.size() > UINT32_MAX ||
            data.BoneMap.empty() ||
            data.BoneMap.size() != data.InverseBindMatrices.size() ||
            data.Materials.empty() ||
            invalidMaterial)
        {
            EE_CORE_ERROR("SkeletalMesh requires geometry, materials, a skeleton, and matching skin bindings.")
            return nullptr;
        }

        if (data.ReferencePose.LocalTransforms.empty())
        {
            for (const auto& bone : data.Skeleton->GetBones())
                data.ReferencePose.LocalTransforms.push_back(bone.ReferenceTransform);
        }

        SModelPose referenceModel;
        if (!data.Skeleton->BuildModelPose(data.ReferencePose, referenceModel))
            return nullptr;

        for (size_t i = 0; i < data.BoneMap.size(); ++i)
        {
            if (data.BoneMap[i] >= data.Skeleton->GetBones().size() ||
                !ValidMatrix(data.InverseBindMatrices[i]))
            {
                EE_CORE_ERROR("SkeletalMesh contains an invalid bone mapping or inverse bind matrix.")
                return nullptr;
            }
        }

        for (auto& vertex : data.Vertices)
        {
            const auto& v = vertex;

            if (!FiniteVector(v.Position) ||
                !FiniteVector(v.Normal) ||
                !FiniteVector(v.Tangent) ||
                !FiniteVector(v.TexCoord) ||
                !FiniteVector(v.TexCoord1) ||
                !FiniteVector(vertex.Weights))
            {
                EE_CORE_ERROR("SkeletalMesh vertex attributes must be finite.")
                return nullptr;
            }

            double total = 0;

            for (int i = 0; i < 4; ++i)
            {
                if (vertex.Weights[i] < 0 ||
                   (vertex.Weights[i] > 0 && vertex.Joints[i] >= data.BoneMap.size()))
                {
                    EE_CORE_ERROR("SkeletalMesh contains an invalid skin influence.")
                    return nullptr;
                }

                if (vertex.Weights[i] == 0)
                    vertex.Joints[i] = 0;

                total += vertex.Weights[i];
            }

            if (total <= 0)
            {
                EE_CORE_ERROR("SkeletalMesh vertices require at least one positive skin weight.")
                return nullptr;
            }

            for (int i = 0; i < 4; ++i)
                vertex.Weights[i] = float(vertex.Weights[i] / total);
        }

        for (const auto& section : data.Sections)
        {
            if (section.MaterialIndex >= data.Materials.size() ||
                section.IndexCount == 0 ||
                section.IndexCount % 3 != 0 ||
                section.FirstIndex > data.Indices.size() ||
                section.IndexCount > data.Indices.size() - section.FirstIndex ||
                section.VertexOffset >= data.Vertices.size())
            {
                EE_CORE_ERROR("SkeletalMesh contains an invalid section.")
                return nullptr;
            }

            const auto indexCount = size_t(section.FirstIndex) + section.IndexCount;
            for (size_t i = section.FirstIndex; i < indexCount; ++i)
            {
                if (data.Indices[i] >= data.Vertices.size() - section.VertexOffset)
                {
                    EE_CORE_ERROR("SkeletalMesh section references a vertex outside the mesh.")
                    return nullptr;
                }
            }
        }

        for (const auto& animation : data.Animations)
        {
            if (!animation || !data.Skeleton->IsCompatible(*animation->GetSkeleton()))
            {
                EE_CORE_ERROR("SkeletalMesh animations must use a compatible skeleton.")
                return nullptr;
            }
        }

        auto mesh = Ref<SkeletalMesh>(new SkeletalMesh(std::move(data)));

        std::vector<SStaticMeshVertex> vertices;
        if (!mesh->SkinVertices(referenceModel, vertices))
            return nullptr;

        mesh->m_LocalBounds = {
            .Min = glm::vec3(std::numeric_limits<float>::max()),
            .Max = glm::vec3(std::numeric_limits<float>::lowest()),
        };

        for (auto& section : mesh->m_Data.Sections)
        {
            section.LocalBounds = {
                .Min = glm::vec3(std::numeric_limits<float>::max()),
                .Max = glm::vec3(std::numeric_limits<float>::lowest()),
            };

            const auto indexCount = size_t(section.FirstIndex) + section.IndexCount;
            for (size_t i = section.FirstIndex; i < indexCount; ++i)
            {
                const auto index = size_t(section.VertexOffset) + mesh->m_Data.Indices[i];
                const auto position = vertices[index].Position;
                section.LocalBounds.Min = glm::min(section.LocalBounds.Min, position);
                section.LocalBounds.Max = glm::max(section.LocalBounds.Max, position);
            }

            mesh->m_LocalBounds.Min = glm::min(mesh->m_LocalBounds.Min, section.LocalBounds.Min);
            mesh->m_LocalBounds.Max = glm::max(mesh->m_LocalBounds.Max, section.LocalBounds.Max);
        }

        return mesh;
    }

    SkeletalMesh::SkeletalMesh(SSkeletalMeshData data)
      : m_Data(std::move(data)) {}

    bool SkeletalMesh::SkinVertices(
        const SModelPose& modelPose,
        std::vector<SStaticMeshVertex>& output
    ) const
    {
        if (!modelPose.IsValid(GetSkeleton()->GetBones().size()))
        {
            EE_CORE_ERROR("SkeletalMesh requires one model transform per skeleton bone.")
            return false;
        }

        std::vector<glm::mat4> palette(m_Data.BoneMap.size());
        for (size_t i = 0; i < palette.size(); ++i)
        {
            palette[i] = modelPose.Matrices[m_Data.BoneMap[i]] * m_Data.InverseBindMatrices[i];
            if (!ValidMatrix(palette[i]))
            {
                EE_CORE_ERROR("SkeletalMesh received an invalid model pose.")
                return false;
            }
        }

        std::vector<SStaticMeshVertex> result;
        result.reserve(m_Data.Vertices.size());

        for (const auto& source : m_Data.Vertices)
        {
            glm::mat4 skin(0.0f);

            for (int i = 0; i < 4; ++i)
                if (source.Weights[i] > 0)
                    skin += palette[source.Joints[i]] * source.Weights[i];

            if (!ValidMatrix(skin))
            {
                EE_CORE_ERROR("SkeletalMesh skinning produced a singular transform.")
                return false;
            }

            SStaticMeshVertex vertex{
                .Position = source.Position,
                .Normal = source.Normal,
                .Tangent = source.Tangent,
                .TexCoord = source.TexCoord,
                .TexCoord1 = source.TexCoord1,
            };

            vertex.Position = glm::vec3(skin * glm::vec4(vertex.Position, 1.0f));

            auto normal = glm::inverseTranspose(glm::mat3(skin)) * vertex.Normal;
            if (glm::dot(normal, normal) < 1e-12f)
                normal = { 0.0f, 1.0f, 0.0f };
            vertex.Normal = glm::normalize(normal);

            auto tangent = glm::mat3(skin) * glm::vec3(vertex.Tangent);
            tangent -= vertex.Normal * glm::dot(vertex.Normal, tangent);

            if (glm::dot(tangent, tangent) < 1e-12f)
            {
                const auto referenceAxis = std::abs(vertex.Normal.z) < 0.9f
                    ? glm::vec3(0, 0, 1)
                    : glm::vec3(0, 1, 0);
                tangent = glm::cross(vertex.Normal, referenceAxis);
            }

            const auto handedness = glm::determinant(glm::mat3(skin)) < 0
                ? -1.0f
                : 1.0f;

            vertex.Tangent = glm::vec4(
                glm::normalize(tangent),
                vertex.Tangent.w * handedness
            );

            if (!FiniteVector(vertex.Position) ||
                !FiniteVector(vertex.Normal) ||
                !FiniteVector(vertex.Tangent))
            {
                EE_CORE_ERROR("SkeletalMesh skinning exceeded the supported vertex range.")
                return false;
            }

            result.push_back(vertex);
        }

        output = std::move(result);

        return true;
    }
}
