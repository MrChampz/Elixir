#include <gtest/gtest.h>

#include <Engine/Material/MaterialLibrary.h>

using namespace Elixir;

TEST(MaterialLibraryTest, RegistersAndFindsDefaultMaterials)
{
    const MaterialLibrary library{ nullptr };

    for (const auto usage : {
        EMaterialUsage::ParticleSprite,
        EMaterialUsage::ParticleRibbon,
        EMaterialUsage::ParticleMesh
    })
    {
        const auto& material = library.GetDefault(usage);
        ASSERT_TRUE(material);
        EXPECT_EQ(library.Find(material->GetName()), material);
        EXPECT_TRUE(material->SupportsUsage(usage));
        EXPECT_TRUE(material->ValidateGraph());
        EXPECT_TRUE(material->GetParameters().empty());
    }
}

TEST(MaterialLibraryTest, RejectsDuplicateMaterialNames)
{
    MaterialLibrary library{ nullptr };
    EXPECT_TRUE(library.Register(CreateRef<Material>("Game.Custom")));
    EXPECT_FALSE(library.Register(CreateRef<Material>("Game.Custom")));
}