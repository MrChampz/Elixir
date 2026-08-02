#include <gtest/gtest.h>

#include <Engine/Material/MaterialRenderer.h>

using namespace Elixir;

TEST(MaterialRendererTest, MapsParticlePassesToMaterialUsages)
{
    EXPECT_EQ(
        MaterialRenderer::GetUsage(EMaterialPass::ParticleSprite),
        EMaterialUsage::ParticleSprite
    );
    EXPECT_EQ(
        MaterialRenderer::GetUsage(EMaterialPass::ParticleRibbon),
        EMaterialUsage::ParticleRibbon
    );
    EXPECT_EQ(
        MaterialRenderer::GetUsage(EMaterialPass::ParticleMesh),
        EMaterialUsage::ParticleMesh
    );
}

TEST(MaterialRendererTest, RejectsAndIncompleteDrawRequest)
{
    EXPECT_FALSE(static_cast<bool>(SMaterialDrawRequest{}));
}