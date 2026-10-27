#include <gtest/gtest.h>
#include <Engine/Mesh/MeshLoaderRegistry.h>

using namespace Elixir;

namespace
{
    class TestMeshLoader final : public MeshLoader
    {
    public:
        std::optional<SStaticMeshData> LoadStaticMesh(const GraphicsContext&, const std::filesystem::path&) const override
        {
            return SStaticMeshData{};
        }

        std::optional<SSkeletalMeshData> LoadSkeletalMesh(const GraphicsContext&, const std::filesystem::path&,
            Ref<const Skeleton>) const override
        {
            return SSkeletalMeshData{};
        }
    };
}

TEST(MeshLoaderRegistryTest, RegistrationKeepsOneImplementationForBothMeshTypes)
{
    MeshLoaderRegistry::Shutdown();
    EXPECT_TRUE(MeshLoaderRegistry::RegisterLoader(CreateScope<TestMeshLoader>()));
    EXPECT_FALSE(MeshLoaderRegistry::RegisterLoader(CreateScope<TestMeshLoader>()));
    EXPECT_TRUE(MeshLoaderRegistry::ReplaceLoader(CreateScope<TestMeshLoader>()));
    MeshLoaderRegistry::Shutdown();
    EXPECT_TRUE(MeshLoaderRegistry::RegisterLoader(CreateScope<TestMeshLoader>()));
    MeshLoaderRegistry::Shutdown();
}

TEST(MeshLoaderRegistryTest, RejectsNullLoadersWithoutReplacingTheActiveLoader)
{
    MeshLoaderRegistry::Shutdown();
    EXPECT_FALSE(MeshLoaderRegistry::RegisterLoader(nullptr));
    EXPECT_TRUE(MeshLoaderRegistry::RegisterLoader(CreateScope<TestMeshLoader>()));
    EXPECT_FALSE(MeshLoaderRegistry::ReplaceLoader(nullptr));
    EXPECT_FALSE(MeshLoaderRegistry::RegisterLoader(CreateScope<TestMeshLoader>()));
    MeshLoaderRegistry::Shutdown();
}

TEST(MeshLoaderRegistryTest, BothEntryPointsRequireAnInitializedGraphicsContext)
{
    MeshLoaderRegistry::Shutdown();
    EXPECT_FALSE(MeshLoaderRegistry::LoadStaticMesh("missing.gltf"));
    EXPECT_FALSE(MeshLoaderRegistry::LoadSkeletalMesh("missing.glb"));
    EXPECT_EQ(MeshLoaderRegistry::GetGeometryPool(), nullptr);
}
