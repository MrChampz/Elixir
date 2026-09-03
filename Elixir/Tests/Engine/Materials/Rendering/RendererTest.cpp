#include <gtest/gtest.h>

#include <Engine/Materials/Rendering/Renderer.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Rendering;

TEST(RendererTest, MapsParticlePassesToMaterialUsages)
{
    EXPECT_EQ(
        Renderer::GetUsage(EMaterialPass::ParticleSprite),
        EMaterialUsage::ParticleSprite
    );
    EXPECT_EQ(
        Renderer::GetUsage(EMaterialPass::ParticleRibbon),
        EMaterialUsage::ParticleRibbon
    );
    EXPECT_EQ(
        Renderer::GetUsage(EMaterialPass::ParticleMesh),
        EMaterialUsage::ParticleMesh
    );
}
