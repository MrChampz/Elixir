#include <gtest/gtest.h>

#include <Engine/Materials/Compilation/CompilationCache.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Compilation;

TEST(CompilationCacheTest, ReusesACompiledMaterialUntilTheSourceRevisionChanges)
{
    CompilationCache cache{ nullptr };
    const auto material = CreateRef<Material>("Cache test", EMaterialUsage::Particle);

    const auto first = cache.GetOrCompile(material);
    const auto second = cache.GetOrCompile(material);

    ASSERT_TRUE(first);
    EXPECT_EQ(first, second);

    ASSERT_TRUE(material->SetBlendMode(EMaterialBlendMode::Translucent));

    const auto rebuilt = cache.GetOrCompile(material);
    ASSERT_TRUE(rebuilt);

    EXPECT_NE(first, rebuilt);
    EXPECT_EQ(rebuilt->GetUsage(), EMaterialUsage::Particle);
}

TEST(CompilationCacheTest, ChangingNormalSpaceInvalidatesAllStaticVariants)
{
    CompilationCache cache(nullptr);
    const auto material = CreateRef<Material>("NormalSpace");
    ASSERT_TRUE(material->UsesTangentSpaceNormal());
    const auto revision = material->GetRevision();
    const auto first = cache.GetOrCompile(material, {{"Feature", true}});
    const auto second = cache.GetOrCompile(material, {{"Feature", false}});
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);

    ASSERT_TRUE(material->SetTangentSpaceNormal(false));
    EXPECT_FALSE(material->UsesTangentSpaceNormal());
    EXPECT_EQ(material->GetRevision(), revision + 1);
    const auto worldFirst = cache.GetOrCompile(material, {{"Feature", true}});
    const auto worldSecond = cache.GetOrCompile(material, {{"Feature", false}});
    ASSERT_TRUE(worldFirst);
    ASSERT_TRUE(worldSecond);
    EXPECT_NE(worldFirst, first);
    EXPECT_NE(worldSecond, second);
    EXPECT_EQ(worldFirst->MaterialRevision, material->GetRevision());

    EXPECT_FALSE(material->SetTangentSpaceNormal(false));
    EXPECT_EQ(material->GetRevision(), revision + 1);
    EXPECT_EQ(cache.GetOrCompile(material, {{"Feature", true}}), worldFirst);

    ASSERT_TRUE(material->SetTangentSpaceNormal(true));
    EXPECT_TRUE(material->UsesTangentSpaceNormal());
    EXPECT_EQ(material->GetRevision(), revision + 2);
    const auto restored = cache.GetOrCompile(material, {{"Feature", true}});
    ASSERT_TRUE(restored);
    EXPECT_NE(restored, worldFirst);
}
