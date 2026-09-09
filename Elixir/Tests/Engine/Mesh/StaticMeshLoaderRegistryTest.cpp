#include <gtest/gtest.h>

#include <Engine/Mesh/StaticMeshLoaderRegistry.h>

using namespace Elixir;

namespace
{
    class TestStaticMeshLoader final : public StaticMeshLoader
    {
    public:
        SStaticMeshLoadResult Load(const SStaticMeshLoadRequest&) const override
        {
            return {};
        }
    };
}

TEST(StaticMeshLoaderRegistryTest, RegistrationKeepsOneActiveImplementation)
{
    StaticMeshLoaderRegistry::Shutdown();

    EXPECT_TRUE(StaticMeshLoaderRegistry::RegisterLoader(CreateScope<TestStaticMeshLoader>()));
    EXPECT_FALSE(StaticMeshLoaderRegistry::RegisterLoader(CreateScope<TestStaticMeshLoader>()));
    EXPECT_TRUE(StaticMeshLoaderRegistry::ReplaceLoader(CreateScope<TestStaticMeshLoader>()));

    StaticMeshLoaderRegistry::Shutdown();
}
