#include <gtest/gtest.h>

#include <Engine/Graphics/Material/MaterialShaderPermutation.h>

using namespace Elixir;

TEST(MaterialShaderPermutation, ExpandsSelectedSurfaceUsagesDeterministically)
{
    const auto permutations = MaterialShaderPermutation::Expand(
        EMaterialDomain::Surface,
        EMaterialUsage::StaticMesh | EMaterialUsage::InstancedMesh
            | EMaterialUsage::ParticleMesh);

    ASSERT_EQ(permutations.size(), 3u);
    EXPECT_EQ(permutations[0].Usage, EMaterialUsage::StaticMesh);
    EXPECT_EQ(permutations[0].VertexFactory, EMaterialVertexFactory::StaticMesh);
    EXPECT_EQ(permutations[1].Usage, EMaterialUsage::InstancedMesh);
    EXPECT_EQ(permutations[1].VertexFactory, EMaterialVertexFactory::InstancedMesh);
    EXPECT_EQ(permutations[2].Usage, EMaterialUsage::ParticleMesh);
    EXPECT_EQ(permutations[2].VertexFactory, EMaterialVertexFactory::ParticleMesh);
    for (const auto& permutation : permutations)
    {
        EXPECT_EQ(permutation.Domain, EMaterialDomain::Surface);
        EXPECT_EQ(permutation.Pass, EMaterialShaderPass::BasePass);
    }
}

TEST(MaterialShaderPermutation, RepresentsFutureDomainContractsWithoutCompilingThem)
{
    const auto postProcess = MaterialShaderPermutation::Expand(
        EMaterialDomain::PostProcess, EMaterialUsage::None);
    ASSERT_EQ(postProcess.size(), 1u);
    EXPECT_EQ(postProcess[0].VertexFactory, EMaterialVertexFactory::FullscreenTriangle);
    EXPECT_EQ(postProcess[0].Pass, EMaterialShaderPass::PostProcess);
    EXPECT_FALSE(MaterialShaderPermutation::IsSupported(postProcess[0]));

    const auto userInterface = MaterialShaderPermutation::Expand(
        EMaterialDomain::UserInterface, EMaterialUsage::None);
    ASSERT_EQ(userInterface.size(), 1u);
    EXPECT_EQ(userInterface[0].VertexFactory, EMaterialVertexFactory::UserInterface);
    EXPECT_EQ(userInterface[0].Pass, EMaterialShaderPass::UserInterface);
    EXPECT_FALSE(MaterialShaderPermutation::IsSupported(userInterface[0]));
}

TEST(MaterialShaderPermutation, MatchesTheMaterialDeclarationAndRendererContract)
{
    const auto& staticMesh = MaterialShaderPermutation::SurfaceStaticMesh();
    EXPECT_TRUE(MaterialShaderPermutation::Matches(
        EMaterialDomain::Surface, EMaterialUsage::StaticMesh, staticMesh));
    EXPECT_TRUE(MaterialShaderPermutation::Matches(
        EMaterialDomain::Surface,
        EMaterialUsage::StaticMesh | EMaterialUsage::SkeletalMesh, staticMesh));
    EXPECT_FALSE(MaterialShaderPermutation::Matches(
        EMaterialDomain::Surface, EMaterialUsage::SkeletalMesh, staticMesh));
    EXPECT_FALSE(MaterialShaderPermutation::Matches(
        EMaterialDomain::UserInterface, EMaterialUsage::None, staticMesh));
    EXPECT_TRUE(MaterialShaderPermutation::IsSupported(staticMesh));
}
