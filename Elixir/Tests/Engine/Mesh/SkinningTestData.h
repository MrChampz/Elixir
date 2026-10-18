#pragma once

#include <Engine/Mesh/SkeletalMeshInstance.h>
#include <Engine/Mesh/StaticMesh.h>
#include <Engine/Materials/DefaultMaterials.h>
#include <glm/gtc/matrix_transform.hpp>

namespace SkinningTests
{
    inline Elixir::Ref<Elixir::SkeletalMesh> MakeMesh(const uint32_t vertexCount = 3)
    {
        using namespace Elixir;
        SSkeletalMeshData data;
        data.Name = "ComputeSkinning";
        data.Skeleton = Skeleton::Create("Rig", {{.Name = "Root"}, {.Name = "Child", .ParentIndex = 0}});
        data.BoneMap = {1, 0};
        data.InverseBindMatrices = {glm::translate(glm::mat4(1), glm::vec3(-1, 0, 0)), glm::mat4(1)};
        data.Vertices.resize(vertexCount);
        for (uint32_t i = 0; i < vertexCount; ++i)
        {
            auto& vertex = data.Vertices[i];
            vertex.Position = {float(i % 3), float(i / 3), -float(i % 7)};
            vertex.Normal = glm::normalize(glm::vec3(1, 1, 1));
            vertex.Tangent = {glm::normalize(glm::vec3(1, -1, 0)), i % 2 ? -1.0f : 1.0f};
            vertex.TexCoord = {0.25f, 0.75f};
            vertex.TexCoord1 = {0.125f, 0.875f};
            vertex.Joints = {0, 1, 0, 1};
            vertex.Weights = {0.4f, 0.3f, 0.2f, 0.1f};
        }
        data.Indices = {0, 1, 2};
        data.Sections = {{.IndexCount = 3}};
        data.Materials = {Materials::CreateDefaultMaterials()[size_t(Materials::EMaterialUsage::Surface)]};
        return SkeletalMesh::Create(std::move(data));
    }
}
