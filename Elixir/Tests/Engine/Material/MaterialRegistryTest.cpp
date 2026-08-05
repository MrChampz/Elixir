#include <gtest/gtest.h>

#include <Engine/Material/MaterialRegistry.h>

using namespace Elixir;

TEST(MaterialRegistryTest, RegistersAndFindsDefaultMaterials)
{
    const MaterialRegistry registry;

    for (const auto usage : {
        EMaterialUsage::ParticleSprite,
        EMaterialUsage::ParticleRibbon,
        EMaterialUsage::ParticleMesh
    })
    {
        const auto& material = registry.GetDefault(usage);
        ASSERT_TRUE(material);
        EXPECT_EQ(registry.Find(material->GetName()), material);
        EXPECT_TRUE(material->SupportsUsage(usage));
        EXPECT_TRUE(material->ValidateGraph());
        EXPECT_TRUE(material->GetParameters().empty());
    }
}

TEST(MaterialRegistryTest, RejectsDuplicateMaterialNames)
{
    MaterialRegistry registry;
    EXPECT_TRUE(registry.Register(CreateRef<Material>("Game.Custom")));
    EXPECT_FALSE(registry.Register(CreateRef<Material>("Game.Custom")));
}