#include <gtest/gtest.h>

#include <Engine/Material/ParticleMaterialDefaults.h>

using namespace Elixir;

TEST(ParticleMaterialDefaultsTest, CreatesAValidMaterialForEachParticleUsage)
{
    for (const auto usage: {
        EMaterialUsage::ParticleSprite,
        EMaterialUsage::ParticleRibbon,
        EMaterialUsage::ParticleMesh,
    })
    {
        const auto material = CreateDefaultParticleMaterial(usage);

        ASSERT_TRUE(material);
        EXPECT_TRUE(material->SupportsUsage(usage));
        EXPECT_TRUE(material->ValidateGraph());
    }
}

TEST(ParticleMaterialDefaultsTest, AuthorsSpriteTextureIntoBaseColorAndOpacity)
{
    const auto material = CreateDefaultParticleMaterial(EMaterialUsage::ParticleSprite);
    ASSERT_NE(material->FindParameter(std::string(DEFAULT_SPRITE_TEXTURE_PARAMETER)), nullptr);
    EXPECT_TRUE(material->ValidateGraph());
}