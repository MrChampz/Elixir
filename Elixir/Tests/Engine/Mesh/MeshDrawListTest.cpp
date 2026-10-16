#include <gtest/gtest.h>
#include <Engine/Mesh/MeshDrawList.h>
#include <Engine/Mesh/StaticMesh.h>
#include <Engine/Materials/Material.h>

using namespace Elixir;
using namespace Elixir::Materials;

namespace
{
    std::vector<SStaticMeshVertex> MakeTriangles()
    {
        std::vector<SStaticMeshVertex> vertices(9);
        for (size_t i = 0; i < vertices.size(); ++i)
            vertices[i].Position = glm::vec3(float(i % 3), 0, -float(i / 3 + 1));
        return vertices;
    }
}

TEST(MeshDrawListTest, InterleavesMaterialsByDeformedTriangleDepth)
{
    auto vertices = MakeTriangles();
    const std::vector<uint32_t> indices{0, 1, 2, 3, 4, 5, 0, 1, 2};
    const std::vector<SMeshSection> sections{
        {.FirstIndex = 0, .IndexCount = 6, .MaterialIndex = 0},
        {.FirstIndex = 6, .IndexCount = 3, .VertexOffset = 6, .MaterialIndex = 1},
    };
    const std::vector modes{EMaterialBlendMode::Translucent, EMaterialBlendMode::Translucent};

    // Move one wing triangle behind the other material after skinning.
    for (size_t i = 0; i < 3; ++i) vertices[i].Position.z = -4;

    const auto draws = BuildMeshDrawList(vertices, indices, sections, modes, glm::mat4(1));
    EXPECT_EQ(draws.Indices, (std::vector<uint32_t>{0, 1, 2, 6, 7, 8, 3, 4, 5}));
    ASSERT_EQ(draws.Ranges.size(), 3);
    EXPECT_EQ(draws.Ranges[0].MaterialIndex, 0);
    EXPECT_EQ(draws.Ranges[1].MaterialIndex, 1);
    EXPECT_EQ(draws.Ranges[2].MaterialIndex, 0);
    EXPECT_FLOAT_EQ(draws.Ranges[0].SortDepth, 4);
    EXPECT_EQ(draws.Ranges[1].FirstIndex, 3);
    EXPECT_EQ(draws.Ranges[2].FirstIndex, 6);
}

TEST(MeshDrawListTest, ReordersTrianglesWhenCameraChangesAndMergesAdjacentMaterial)
{
    const auto vertices = MakeTriangles();
    const std::vector<uint32_t> indices{0, 1, 2, 3, 4, 5, 6, 7, 8};
    const std::vector<SMeshSection> sections{{.IndexCount = 9}};
    const std::vector modes{EMaterialBlendMode::Translucent};
    const auto draws = BuildMeshDrawList(vertices, indices, sections, modes, glm::mat4(1));
    EXPECT_EQ(draws.Indices, (std::vector<uint32_t>{6, 7, 8, 3, 4, 5, 0, 1, 2}));
    ASSERT_EQ(draws.Ranges.size(), 1);
    EXPECT_EQ(draws.Ranges[0].IndexCount, 9);

    auto view = glm::mat4(1);
    view[2][2] = -1;
    EXPECT_EQ(BuildMeshDrawList(vertices, indices, sections, modes, view).Indices, indices);
}

TEST(MeshDrawListTest, PreservesOpaqueAndMaskedTriangleOrderAndSectionOffsets)
{
    const auto vertices = MakeTriangles();
    const std::vector<uint32_t> indices{0, 1, 2, 0, 1, 2, 0, 1, 2};
    const std::vector<SMeshSection> sections{
        {.FirstIndex = 0, .IndexCount = 3, .VertexOffset = 3, .MaterialIndex = 0},
        {.FirstIndex = 3, .IndexCount = 3, .VertexOffset = 6, .MaterialIndex = 1},
        {.FirstIndex = 6, .IndexCount = 3, .MaterialIndex = 2},
    };
    const std::vector modes{EMaterialBlendMode::Translucent, EMaterialBlendMode::Opaque, EMaterialBlendMode::Masked};
    const auto draws = BuildMeshDrawList(vertices, indices, sections, modes, glm::mat4(1));
    EXPECT_EQ(draws.Indices, (std::vector<uint32_t>{6, 7, 8, 0, 1, 2, 3, 4, 5}));
    ASSERT_EQ(draws.Ranges.size(), 3);
    EXPECT_EQ(draws.Ranges[0].FirstIndex, 0);
    EXPECT_EQ(draws.Ranges[1].FirstIndex, 3);
    EXPECT_EQ(draws.Ranges[2].FirstIndex, 6);
    EXPECT_EQ(draws.Ranges[0].MaterialIndex, 1);
    EXPECT_EQ(draws.Ranges[1].MaterialIndex, 2);
    EXPECT_EQ(draws.Ranges[2].MaterialIndex, 0);
}

TEST(MeshDrawListTest, KeepsEqualDepthTrianglesStableAndSkipsEmptySections)
{
    auto vertices = MakeTriangles();
    for (auto& vertex : vertices) vertex.Position.z = -1;
    const std::vector<uint32_t> indices{3, 4, 5, 0, 1, 2};
    const std::vector<SMeshSection> sections{{.IndexCount = 0}, {.IndexCount = 6}};
    const std::vector modes{EMaterialBlendMode::Translucent};
    EXPECT_EQ(BuildMeshDrawList(vertices, indices, sections, modes, glm::mat4(1)).Indices, indices);
}
