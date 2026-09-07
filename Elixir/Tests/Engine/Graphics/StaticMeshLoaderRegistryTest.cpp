#include <gtest/gtest.h>

#include <Engine/Graphics/StaticMeshLoaderRegistry.h>

using namespace Elixir;

namespace
{
    class TestGltfStaticMeshLoader final : public StaticMeshLoader
    {
      public:
        EStaticMeshFormat GetFormat() const override { return EStaticMeshFormat::Gltf; }
        SStaticMeshLoadResult Load(const SStaticMeshLoadRequest&) const override { return {}; }
    };
}

TEST(StaticMeshLoaderRegistryTest, RegistrationRejectsDuplicatesAndAllowsReplacement)
{
    StaticMeshLoaderRegistry::Shutdown();

    EXPECT_TRUE(StaticMeshLoaderRegistry::RegisterLoader(CreateScope<TestGltfStaticMeshLoader>()));
    EXPECT_FALSE(StaticMeshLoaderRegistry::RegisterLoader(CreateScope<TestGltfStaticMeshLoader>()));
    EXPECT_TRUE(StaticMeshLoaderRegistry::ReplaceLoader(CreateScope<TestGltfStaticMeshLoader>()));

    StaticMeshLoaderRegistry::Shutdown();
}
