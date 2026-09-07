#include <gtest/gtest.h>

#include <Engine/Materials/Compilation/CompilationCache.h>

using namespace Elixir;
using namespace Elixir::Materials;
using namespace Elixir::Materials::Compilation;

TEST(CompilationCacheTest, ReusesACompiledMaterialUntilTheSourceRevisionChanges)
{
    CompilationCache cache{ nullptr };
    const auto material = CreateRef<Material>("Cache test");

    ASSERT_TRUE(material->SetUsage(EMaterialUsage::ParticleSprite, true));

    const auto first = cache.GetOrCompile(material);
    const auto second = cache.GetOrCompile(material);

    ASSERT_TRUE(first);
    EXPECT_EQ(first, second);

    ASSERT_TRUE(material->SetUsage(EMaterialUsage::ParticleRibbon, true));

    const auto rebuilt = cache.GetOrCompile(material);
    ASSERT_TRUE(rebuilt);

    EXPECT_NE(first, rebuilt);
    EXPECT_TRUE(rebuilt->SupportsUsage(EMaterialUsage::ParticleRibbon));
}
