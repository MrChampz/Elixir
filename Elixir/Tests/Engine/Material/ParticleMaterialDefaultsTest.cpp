#include <gtest/gtest.h>

#include <Engine/Material/ParticleMaterialDefaults.h>

using namespace Elixir;

TEST(ParticleMaterialDefaultsTest, AuthorsConstantsForEachParticleUsage)
{
    for (const auto usage: {
        EMaterialUsage::ParticleSprite,
        EMaterialUsage::ParticleRibbon,
        EMaterialUsage::ParticleMesh,
    })
    {
        const auto material = CreateParticleMaterial({
            .Usage = usage,
            .BaseColor = { 0.25f, 0.5f, 0.75f },
            .Opacity = 0.4f,
            .Emissive = { 1.5f, 0.2f, 0.1f },
        });

        ASSERT_TRUE(material);
        EXPECT_TRUE(material->SupportsUsage(usage));
        EXPECT_TRUE(material->ValidateGraph());

        const auto hlsl = material->GetGraph().GenerateHLSL();
        EXPECT_NE(hlsl.find("surface.BaseColor"), std::string::npos);
        EXPECT_NE(hlsl.find("0.400000"), std::string::npos);
        EXPECT_NE(hlsl.find("surface.Opacity"), std::string::npos);
        EXPECT_NE(hlsl.find("surface.Emissive"), std::string::npos);
    }
}

TEST(ParticleMaterialDefaultsTest, AuthorsSpriteTextureIntoBaseColorAndOpacity)
{
    const auto material = CreateParticleMaterial({
        .Usage = EMaterialUsage::ParticleSprite,
        .BaseColor = { 0.8f, 0.4f, 0.2f },
        .Opacity = 0.6f,
    });
    ASSERT_NE(material->FindParameter(std::string(DEFAULT_SPRITE_TEXTURE_PARAMETER)), nullptr);
    EXPECT_TRUE(material->ValidateGraph());
}